/*
 * MintRIVA - cybergraphics RTG window backend.
 *
 * Opens a titled window on the default public screen (truecolour RTG on
 * SAGA/P96/CGX) and blits RGB24 frames with WritePixelArray, letting
 * cybergraphics do the RGB->screen-depth conversion. Library bases are opened
 * by display.c; this backend just needs CyberGfxBase to be present.
 */
#include "amiga_display.h"
#include "display_backend.h"

#include <stddef.h>
#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/displayinfo.h>
#include <cybergraphx/cybergraphics.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

typedef struct {
    struct Window *win;
    int            bl, bt;   /* blit origin inside the window borders       */
    int            iw, ih;   /* current resizeable inner dimensions         */
    int            pending_w, pending_h;
    clock_t        resize_at;
    unsigned char *scaled;   /* persistent RGB24 destination                */
    size_t         scaled_size;
    int            scaled_w, scaled_h, scaled_stride;
    mr_display_timing timing;
    int            quit;
    const char    *title;    /* last status shown, for idempotent updates      */
} cgx_state;

#define ESC_RAWKEY 0x45

/* Having cybergraphics.library is not enough - the actual public screen we'd
 * render into must be an RTG/truecolour mode. On an AGA (planar) Workbench,
 * WritePixelArray can't draw, so report "not RTG" and let the dispatcher fall
 * back to the AGA backend. */
static int default_screen_is_rtg(void)
{
    struct Screen *scr = LockPubScreen(NULL);
    int rtg = 0;
    if (scr) {
        ULONG modeid = GetVPModeID(&scr->ViewPort);
        if (modeid != (ULONG)INVALID_ID && IsCyberModeID(modeid))
            rtg = 1;
        UnlockPubScreen(NULL, scr);
    }
    return rtg;
}

static void *cgx_open(int w, int h, const char *title)
{
    cgx_state *s;
    if (!CyberGfxBase || !default_screen_is_rtg())
        return NULL;                              /* not RTG -> try AGA      */

    s = (cgx_state *)AllocVec(sizeof *s, MEMF_CLEAR);
    if (!s) return NULL;

    s->win = OpenWindowTags(NULL,
        WA_Title,       (ULONG)(title ? title : "MintRIVA"),
        WA_InnerWidth,  (ULONG)w,
        WA_InnerHeight, (ULONG)h,
        WA_Flags,       WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_CLOSEGADGET |
                        WFLG_SIZEGADGET | WFLG_ACTIVATE | WFLG_NOCAREREFRESH,
        WA_MinWidth,    160,
        WA_MinHeight,   100,
        WA_MaxWidth,    (ULONG)-1,
        WA_MaxHeight,   (ULONG)-1,
        WA_IDCMP,       IDCMP_CLOSEWINDOW | IDCMP_RAWKEY | IDCMP_NEWSIZE,
        TAG_END);
    if (!s->win) { FreeVec(s); return NULL; }

    s->bl = s->win->BorderLeft;
    s->bt = s->win->BorderTop;
    s->iw = s->win->Width - s->win->BorderLeft - s->win->BorderRight;
    s->ih = s->win->Height - s->win->BorderTop - s->win->BorderBottom;
    s->pending_w = s->iw; s->pending_h = s->ih;
    return s;
}

static unsigned long elapsed_us(clock_t begin)
{
    return (unsigned long)((clock() - begin) * 1000000UL / CLOCKS_PER_SEC);
}

static void report_slow(const char *operation, unsigned long usec,
                        mr_display_service_fn service, void *opaque)
{
    if (usec <= 10000) return;
    if (service) service(opaque);
    printf("cgx-slow operation=%s duration=%lu us\n", operation, usec);
    if (service) service(opaque);
}

static int ensure_scaled(cgx_state *s, int w, int h)
{
    size_t stride, bytes;
    unsigned char *p;
    if (w <= 0 || h <= 0 || w > 65535 || h > 65535) return 0;
    if ((size_t)w > (size_t)-1 / 3) return 0;
    stride = (size_t)w * 3;
    if ((size_t)h > (size_t)-1 / stride) return 0;
    bytes = stride * (size_t)h;
    if (s->scaled && s->scaled_w == w && s->scaled_h == h) return 1;
    p = (unsigned char *)AllocVec(bytes, MEMF_ANY);
    if (!p) return 0; /* retain the old buffer and the existing failure mode */
    if (s->scaled) FreeVec(s->scaled);
    s->scaled = p; s->scaled_size = bytes;
    s->scaled_w = w; s->scaled_h = h; s->scaled_stride = (int)stride;
    return 1;
}

/* Integer nearest-neighbour RGB24 scaler. Geometry increments are calculated
 * once per frame (and never per pixel); strips bound time between Paula pumps. */
static void scale_rgb24(cgx_state *s, const unsigned char *src, int sw, int sh,
                        int src_stride, mr_display_service_fn service,
                        void *opaque)
{
    unsigned long xstep = ((unsigned long)sw << 16) / (unsigned long)s->iw;
    unsigned long ystep = ((unsigned long)sh << 16) / (unsigned long)s->ih;
    unsigned long syfp = 0;
    int y;
    for (y = 0; y < s->ih; y++, syfp += ystep) {
        const unsigned char *sp = src + (size_t)(syfp >> 16) * src_stride;
        unsigned char *dp = s->scaled + (size_t)y * s->scaled_stride;
        unsigned long sx = 0;
        int x;
        for (x = 0; x < s->iw; x++, sx += xstep) {
            const unsigned char *p = sp + (size_t)(sx >> 16) * 3;
            *dp++ = p[0]; *dp++ = p[1]; *dp++ = p[2];
        }
        if (service && (y & 15) == 15) service(opaque);
    }
}

static void cgx_show(void *h, const unsigned char *rgb, int w, int hh,
                     int stride, int dy0, int dy1,
                     mr_display_service_fn service, void *service_opaque)
{
    cgx_state *s = (cgx_state *)h;
    clock_t total, mark;
    int native, y;
    if (!s || !s->win) return;
    memset(&s->timing, 0, sizeof s->timing);
    total = mark = clock();
    /* Coalesce the storm of NEWSIZE messages: keep drawing the last stable
     * geometry until Intuition has been quiet for 100 ms. */
    if ((s->pending_w != s->iw || s->pending_h != s->ih) &&
        clock() - s->resize_at >= CLOCKS_PER_SEC / 10) {
        s->iw = s->pending_w; s->ih = s->pending_h;
    }
    s->timing.resize_us = elapsed_us(mark);
    report_slow("resize-handling", s->timing.resize_us, service, service_opaque);
    mark = clock();
    if (dy0 < 0) dy0 = 0;
    if (dy1 > hh) dy1 = hh;
    if (dy1 <= dy0) return;                       /* nothing changed         */
    s->timing.clip_us = elapsed_us(mark);
    report_slow("clipping-setup", s->timing.clip_us, service, service_opaque);
    mark = clock();
    s->timing.src_w = w; s->timing.src_h = hh;
    s->timing.dst_w = s->iw; s->timing.dst_h = s->ih;
    s->timing.src_format = s->timing.dst_format = "RGB24";
    native = s->iw == w && s->ih == hh;
    s->timing.geometry_us = elapsed_us(mark);
    report_slow("geometry-format-setup", s->timing.geometry_us,
                service, service_opaque);
    if (!native) {
        int buffer_reused = s->scaled && s->scaled_w == s->iw &&
                            s->scaled_h == s->ih;
        mark = clock();
        if (service) service(service_opaque);
        if (!ensure_scaled(s, s->iw, s->ih)) return;
        if (service) service(service_opaque);
        s->timing.allocation_us = elapsed_us(mark);
        report_slow(buffer_reused ? "destination-buffer-check"
                                  : "destination-buffer-allocation",
                    s->timing.allocation_us, service, service_opaque);
        mark = clock();
        scale_rgb24(s, rgb, w, hh, stride, service, service_opaque);
        s->timing.scale_us = elapsed_us(mark);
        report_slow("software-scale", s->timing.scale_us,
                    service, service_opaque);
        s->timing.pixels = (unsigned long)s->iw * (unsigned long)s->ih;
        s->timing.bytes = (unsigned long)s->scaled_size;
        s->timing.copies = 1;
        rgb = s->scaled; stride = s->scaled_stride; dy0 = 0; dy1 = s->ih;
        w = s->iw;
    }
    s->timing.prepare_us = elapsed_us(total);
    if (service) service(service_opaque);
    mark = clock();
    if (native) {
        /* At native size retain the dirty-row fast path. */
        WritePixelArray((APTR)(rgb + (size_t)dy0 * stride), 0, 0,
                        (UWORD)stride, s->win->RPort, (UWORD)s->bl,
                        (UWORD)(s->bt + dy0), (UWORD)w,
                        (UWORD)(dy1 - dy0), RECTFMT_RGB);
    } else {
        /* Band large transfers so audio.device is serviced even when the RTG
         * driver copies slowly. Each source row is transferred exactly once. */
        for (y = 0; y < s->ih; y += 32) {
            int rows = s->ih - y < 32 ? s->ih - y : 32;
            WritePixelArray((APTR)(rgb + (size_t)y * stride), 0, 0,
                            (UWORD)stride, s->win->RPort, (UWORD)s->bl,
                            (UWORD)(s->bt + y), (UWORD)w, (UWORD)rows,
                            RECTFMT_RGB);
            if (service) service(service_opaque);
        }
    }
    s->timing.blit_us = elapsed_us(mark);
    if (service) service(service_opaque);
    s->timing.total_us = elapsed_us(total);
}

static int cgx_timing(void *h, mr_display_timing *timing)
{
    cgx_state *s = (cgx_state *)h;
    if (!s || !timing) return 0;
    *timing = s->timing; return 1;
}

static int cgx_poll(void *h)
{
    cgx_state *s = (cgx_state *)h;
    struct IntuiMessage *msg;
    int ev = MR_EV_NONE;
    if (!s || !s->win) return MR_EV_QUIT;
    while ((msg = (struct IntuiMessage *)GetMsg(s->win->UserPort))) {
        ULONG cls = msg->Class; UWORD code = msg->Code;
        ReplyMsg((struct Message *)msg);
        if (cls == IDCMP_CLOSEWINDOW) s->quit = 1;
        else if (cls == IDCMP_NEWSIZE) {
            s->pending_w = s->win->Width - s->win->BorderLeft - s->win->BorderRight;
            s->pending_h = s->win->Height - s->win->BorderTop - s->win->BorderBottom;
            s->resize_at = clock();
        }
        else if (cls == IDCMP_RAWKEY && !(code & 0x80)) {  /* key down only  */
            switch (code) {
            case 0x45: s->quit = 1; break;             /* ESC              */
            case 0x40: ev = MR_EV_PAUSE; break;        /* space            */
            case 0x4E: ev = MR_EV_SEEK_FWD; break;     /* cursor right     */
            case 0x4F: ev = MR_EV_SEEK_BACK; break;    /* cursor left      */
            }
        }
    }
    return s->quit ? MR_EV_QUIT : ev;
}

static void cgx_status(void *h, const char *text)
{
    cgx_state *s = (cgx_state *)h;
    const char *title = (text && *text) ? text : "MintRIVA";
    if (!s || !s->win) return;
    /* Callers pass string literals, so a pointer compare cheaply skips the
     * common case of the same status being set repeatedly (e.g. every
     * reconnect attempt). The second arg leaves the screen title unchanged. */
    if (s->title == title) return;
    s->title = title;
    SetWindowTitles(s->win, (CONST_STRPTR)title, (CONST_STRPTR)~0UL);
}

static void cgx_close(void *h)
{
    cgx_state *s = (cgx_state *)h;
    if (!s) return;
    if (s->scaled) FreeVec(s->scaled);
    if (s->win) CloseWindow(s->win);
    FreeVec(s);
}

const display_backend backend_cgx = {
    "RTG (CGX)", cgx_open, cgx_show, cgx_timing, cgx_poll, cgx_close, cgx_status
};
