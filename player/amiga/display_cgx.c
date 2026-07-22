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

typedef struct {
    struct Window *win;
    int            bl, bt;   /* blit origin inside the window borders       */
    int            quit;
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
                        WFLG_ACTIVATE | WFLG_NOCAREREFRESH,
        WA_IDCMP,       IDCMP_CLOSEWINDOW | IDCMP_RAWKEY,
        TAG_END);
    if (!s->win) { FreeVec(s); return NULL; }

    s->bl = s->win->BorderLeft;
    s->bt = s->win->BorderTop;
    return s;
}

static void cgx_show(void *h, const unsigned char *rgb, int w, int hh,
                     int stride, int dy0, int dy1)
{
    cgx_state *s = (cgx_state *)h;
    if (!s || !s->win) return;
    if (dy0 < 0) dy0 = 0;
    if (dy1 > hh) dy1 = hh;
    if (dy1 <= dy0) return;                       /* nothing changed         */
    /* blit only the changed band, sourced from that row of the frame */
    WritePixelArray((APTR)(rgb + (size_t)dy0 * stride), 0, 0, (UWORD)stride,
                    s->win->RPort, (UWORD)s->bl, (UWORD)(s->bt + dy0),
                    (UWORD)w, (UWORD)(dy1 - dy0), RECTFMT_RGB);
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

static void cgx_close(void *h)
{
    cgx_state *s = (cgx_state *)h;
    if (!s) return;
    if (s->win) CloseWindow(s->win);
    FreeVec(s);
}

const display_backend backend_cgx = {
    "RTG (CGX)", cgx_open, cgx_show, cgx_poll, cgx_close
};
