/*
 * MintVID - AGA (planar) display backend.
 *
 * Opens a custom screen and blits each frame through a portable pixel encoder
 * (256-colour dither / HAM8 / HAM6) plus a chunky->planar step. The default
 * WritePixelArray8 remains the default; --c2p selects the portable mr_c2p8 and
 * --riva-c2p selects an opt-in 32-pixel, direct-to-plane variant for hardware
 * measurement. --kalms-c2p selects Kalms' public-domain 68030 converter.
 * Optional 2x pixel doubling.
 */
#include "amiga_display.h"
#include "display_backend.h"
#include "../core/mr_dither.h"
#include "../core/mr_ham.h"
#include "../core/mr_scale.h"
#include "../core/mr_c2p.h"
#include "../vendor/kalms-c2p/normal/c2p1x1_8_c5_030.h"

#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/rastport.h>
#include <graphics/view.h>
#include <graphics/displayinfo.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>

#include <stdlib.h>
#include <time.h>

#define ESC_RAWKEY 0x45

/* Encode vs blit time, for mrplay --time. */
static clock_t s_enc = 0, s_blit = 0, s_frame_enc = 0, s_frame_blit = 0;
static int s_kalms_active = 0;
void display_aga_timing(unsigned long *enc_ms, unsigned long *blit_ms)
{
    if (enc_ms)  *enc_ms  = (unsigned long)(s_enc  * 1000 / CLOCKS_PER_SEC);
    if (blit_ms) *blit_ms = (unsigned long)(s_blit * 1000 / CLOCKS_PER_SEC);
}
void display_aga_frame_timing(unsigned long *enc_ms, unsigned long *blit_ms)
{
    if (enc_ms)  *enc_ms  = (unsigned long)(s_frame_enc  * 1000 / CLOCKS_PER_SEC);
    if (blit_ms) *blit_ms = (unsigned long)(s_frame_blit * 1000 / CLOCKS_PER_SEC);
}
int display_aga_kalms_timing(unsigned long *conversion_ms)
{
    if (conversion_ms)
        *conversion_ms = (unsigned long)(s_blit * 1000 / CLOCKS_PER_SEC);
    return s_kalms_active;
}

typedef struct {
    struct Screen  *scr;
    struct Window  *win;
    struct RastPort temprp;      /* only used on the WritePixelArray8 path  */
    struct BitMap  *tempbm;
    unsigned char  *enc;         /* 1x encode buffer (scale==2 only)        */
    unsigned char  *chunky;      /* pw*dh pixels to blit                     */
    unsigned char  *scaled;      /* resized RGB (resize only)                */
    int             w, h;
    int             dw, dh;      /* displayed size                          */
    int             pw;          /* chunky row stride (>= dw)               */
    int             depth;
    int             x0, y0, x0byte;
    int             ham, scale, resize, use_c2p, use_riva_c2p, use_kalms_c2p, use_akiko;
    int             quit;
} aga_state;

/*
 * CD32 Akiko hardware chunky->planar. The Akiko chip exposes a C2P port at
 * 0xB80038: feed 8 chunky longwords (32 pixels), then read back 8 planar
 * longwords (one 32-bit slice per bitplane), and store each into its plane.
 * This offloads the transpose from the 020, which is the win on a stock CD32.
 *
 * pw must be a multiple of 32 and x0byte a multiple of 4 (aga_open enforces
 * both when Akiko is active), so every plane store lands longword-aligned.
 *
 * NOTE: the plane/bit ordering of the Akiko handshake here is reconstructed
 * from documentation - it needs verifying on real CD32 hardware and may need
 * the read order (or a bit reversal) tweaked if the picture comes out garbled.
 */
#define AKIKO_C2P_REG 0xB80038
static void akiko_c2p(const uint8_t *chunky, int pw, int h, int chunky_stride,
                      int nplanes, uint8_t *const planes[], int bpr,
                      int x0byte, int y0)
{
    volatile ULONG *ak = (volatile ULONG *)AKIKO_C2P_REG;
    int nbatch = pw >> 5;                          /* 32 pixels per batch     */
    int y;
    for (y = 0; y < h; y++) {
        const ULONG *src = (const ULONG *)(chunky + (size_t)y * chunky_stride);
        int dstrow = (y0 + y) * bpr + x0byte;
        int b;
        for (b = 0; b < nbatch; b++) {
            ULONG planeword[8];
            int i, p;
            for (i = 0; i < 8; i++) *ak = *src++;        /* feed 32 chunky px */
            for (i = 0; i < 8; i++) planeword[i] = *ak;  /* 8 planar slices   */
            for (p = 0; p < nplanes; p++)
                *(ULONG *)(planes[p] + dstrow + b * 4) = planeword[p];
        }
    }
}

static int chipset_has_aga(void)
{
    return GfxBase && (GfxBase->ChipRevBits0 & GFXF_AA_LISA) != 0;
}

/* OCS/ECS indexed output uses a fixed 4x4x2 RGB cube. Keep this native-only
 * encoder here so it does not disturb the established AGA dither hot path. */
static const uint8_t dither5_bayer[4][4] = {
    { 0, 8, 2, 10 }, { 12, 4, 14, 6 },
    { 3, 11, 1, 9 }, { 15, 7, 13, 5 }
};
static uint8_t dither5_r[16][256], dither5_g[16][256],
               dither5_b[16][256];
static int dither5_ready;

static void dither5_palette(uint8_t *pal)
{
    int r, g, b, i = 0;
    for (r = 0; r < 4; r++)
        for (g = 0; g < 4; g++)
            for (b = 0; b < 2; b++) {
                pal[i * 3 + 0] = (uint8_t)(r * 255 / 3);
                pal[i * 3 + 1] = (uint8_t)(g * 255 / 3);
                pal[i * 3 + 2] = (uint8_t)(b * 255);
                i++;
            }
}

static void dither5_build(void)
{
    int t, v;
    for (t = 0; t < 16; t++)
        for (v = 0; v < 256; v++) {
            int v4 = v + (t - 8) * 85 / 16;
            int v2 = v + (t - 8) * 255 / 16;
            if (v4 < 0) v4 = 0; else if (v4 > 255) v4 = 255;
            if (v2 < 0) v2 = 0; else if (v2 > 255) v2 = 255;
            dither5_r[t][v] = (uint8_t)(((v4 * 3 + 127) / 255) * 8);
            dither5_g[t][v] = (uint8_t)(((v4 * 3 + 127) / 255) * 2);
            dither5_b[t][v] = (uint8_t)((v2 + 127) / 255);
        }
    dither5_ready = 1;
}

static void dither5_rgb(const uint8_t *rgb, int w, int h, int rgb_stride,
                        uint8_t *out, int out_stride, int y_base)
{
    int x, y;
    if (!dither5_ready) dither5_build();
    for (y = 0; y < h; y++) {
        const uint8_t *src = rgb + (size_t)y * rgb_stride;
        uint8_t *dst = out + (size_t)y * out_stride;
        const uint8_t *threshold = dither5_bayer[(y_base + y) & 3];
        for (x = 0; x < w; x++) {
            int t = threshold[x & 3];
            dst[x] = (uint8_t)(dither5_r[t][src[x * 3 + 0]] +
                               dither5_g[t][src[x * 3 + 1]] +
                               dither5_b[t][src[x * 3 + 2]]);
        }
    }
}

static void load_palette(struct Screen *scr, int ham, int depth)
{
    ULONG tab[1 + 256 * 3 + 1];
    int   n, i;
    uint8_t pal[256 * 3];
    if (ham) { n = (ham >= 8) ? 64 : 16; mr_ham_palette(pal, ham); }
    else if (depth == 5) {
        n = 32;
        dither5_palette(pal);
    } else {
        n = 256;
        mr_dither_palette(pal);
    }
    tab[0] = ((ULONG)n << 16) | 0;
    for (i = 0; i < n; i++) {
        ULONG r = pal[i*3+0], g = pal[i*3+1], b = pal[i*3+2];
        tab[1 + i*3 + 0] = (r << 24) | (r << 16) | (r << 8) | r;
        tab[1 + i*3 + 1] = (g << 24) | (g << 16) | (g << 8) | g;
        tab[1 + i*3 + 2] = (b << 24) | (b << 16) | (b << 8) | b;
    }
    tab[1 + n*3] = 0;
    LoadRGB32(&scr->ViewPort, tab);
}

static void *aga_open(int w, int h, const char *title)
{
    aga_state *s;
    int   scale = (g_aga_scale == 2) ? 2 : 1;
    int   ham   = g_aga_ham;
    int   akiko = g_aga_akiko;
    int   riva_c2p_mode = (g_aga_c2p == 2) && !akiko;
    int   kalms_c2p_mode = (g_aga_c2p == 3) && !akiko;
    int   c2p   = (g_aga_c2p == 1) && !akiko;
    int   dw, dh, depth;
    int   sw, sh, physical_w, physical_h, hires, lace, resize;
    ULONG modeid;

    s_enc = s_blit = s_frame_enc = s_frame_blit = 0;
    s_kalms_active = 0;

    /* HAM8 and eight indexed planes need AGA. HAM6 is the original Amiga HAM
     * mode and works on OCS/ECS too. Ordinary OCS/ECS output uses a genuine
     * five-plane/32-colour encoder instead of trying to open an impossible
     * eight-plane screen and silently losing the upper index bits. */
    if (ham == 8 && !chipset_has_aga()) {
        printf("planar: HAM8 requires AGA; using HAM6\n");
        ham = 6;
    }
    depth = ham == 6 ? 6 : (chipset_has_aga() ? 8 : 5);

    /*
     * AGA raster pixels are not square: HIRES doubles horizontal resolution
     * and interlace doubles vertical resolution without changing the physical
     * display size.  Fit in a 320x256 "physical pixel" canvas first, then
     * multiply the corresponding raster axis.  Thus 854x480 becomes 640x180
     * in non-laced HIRES, or 640x360 with --lace, instead of the squeezed
     * 427x240 image produced by treating HIRES pixels as square.
     */
    hires = w * scale > 320;
    /* ECS/OCS Denise+Agnus cannot fetch more than 4 bitplanes' worth of data
     * per scanline at HIRES pixel rates - a genuine display DMA bandwidth
     * limit, not something AGA needed since it redesigned the fetch path.
     * This backend's non-AGA paths use depth 5 (dither5_rgb, the ECS/OCS
     * 32-colour cube) or depth 6 (HAM6), both invalid HIRES bitplane counts
     * on real ECS/OCS hardware - OpenScreenTags simply fails for that
     * combination. Confirmed on real ECS hardware: a live stream narrow
     * enough to stay LORES opened fine; two wider ones that crossed the 320
     * threshold into HIRES both failed to open a display at all. Keep ECS/
     * OCS in LORES unconditionally (this backend has no HIRES-capable <=4
     * plane encoder) rather than requesting a mode the chipset cannot do -
     * the picture is downscaled a bit more instead of not appearing. */
    if (!chipset_has_aga()) hires = 0;
    lace = g_aga_lace && h * scale > 256;
    physical_w = w * scale;
    physical_h = h * scale;
    mr_scale_fit_rect(physical_w, physical_h, 320, 256,
                      &physical_w, &physical_h);
    dw = physical_w * (hires ? 2 : 1);
    dh = physical_h * (lace ? 2 : 1);

    /* Preserve the cheap encoded-chunky 2x path when the geometry is exactly
     * 2x.  All aspect compensation goes through the general RGB resizer. */
    if (dw == w * 2 && dh == h * 2) {
        scale = 2;
        resize = 0;
    } else {
        scale = 1;
        resize = (dw != w || dh != h);
    }

    sw = hires ? 640 : 320;
    sh = lace ? ((dh + 15) & ~15) : 256;
    if (sh < 256) sh = 256;
    modeid = hires ? HIRES_KEY : LORES_KEY;
    if (lace) modeid |= LACE;
    if (ham) modeid |= HAM;
    (void)title;

    s = (aga_state *)calloc(1, sizeof *s);
    if (!s) return NULL;
    s->w = w; s->h = h; s->dw = dw; s->dh = dh;
    s->ham = ham; s->scale = scale; s->resize = resize;
    s->depth = depth; s->use_c2p = c2p; s->use_riva_c2p = riva_c2p_mode;
    s->use_kalms_c2p = kalms_c2p_mode;
    s->use_akiko = akiko;
    /* Akiko converts 32 pixels per batch, so it needs a 32-pixel-aligned x and
     * a 32-multiple row stride; the built-in C2P only needs 8-pixel alignment.
     * graphics.library WritePixelArray8 requires each source row rounded up to
     * 16 pixels even though xstop names the unpadded visible width.  Packing an
     * odd width tightly makes every following row start early (854 / 2 = 427
     * exposed this as five-pixel diagonal wraps). */
    if (kalms_c2p_mode)
                   { s->pw = sw; s->x0 = ((sw - dw) / 2) & ~31; }
    else if (akiko || riva_c2p_mode)
                   { s->pw = (dw + 31) & ~31; s->x0 = ((sw - dw) / 2) & ~31; }
    else if (c2p)  { s->pw = (dw + 7)  & ~7;  s->x0 = ((sw - dw) / 2) & ~7;  }
    else           { s->pw = (dw + 15) & ~15; s->x0 = (sw - dw) / 2;         }
    if (s->x0 < 0) s->x0 = 0;
    s->y0 = (sh - dh) / 2;
    s->x0byte = s->x0 >> 3;

    s->scr = OpenScreenTags(NULL,
        SA_Width, (ULONG)sw, SA_Height, (ULONG)sh, SA_Depth, (ULONG)depth,
        SA_DisplayID, modeid, SA_Type, CUSTOMSCREEN,
        SA_Quiet, TRUE, SA_ShowTitle, FALSE, TAG_END);
    if (!s->scr) { free(s); return NULL; }
    load_palette(s->scr, ham, depth);

    s->win = OpenWindowTags(NULL,
        WA_CustomScreen, (ULONG)s->scr,
        WA_Left, 0, WA_Top, 0, WA_Width, (ULONG)sw, WA_Height, (ULONG)sh,
        WA_Flags, WFLG_BORDERLESS | WFLG_BACKDROP | WFLG_ACTIVATE |
                  WFLG_RMBTRAP | WFLG_NOCAREREFRESH,
        WA_IDCMP, IDCMP_RAWKEY, TAG_END);
    if (!s->win) { CloseScreen(s->scr); free(s); return NULL; }

    if (kalms_c2p_mode) {
        struct BitMap *bm = s->scr->RastPort.BitMap;
        long spacing = 0;
        int p, compatible = depth == 8 && bm && bm->Planes[0] && bm->Planes[1] &&
                            bm->BytesPerRow == s->pw / 8;
        if (compatible) {
            spacing = (long)((ULONG)bm->Planes[1] - (ULONG)bm->Planes[0]);
            compatible = spacing > 0 && spacing <= 16384;
        }
        for (p = 1; compatible && p < depth; p++)
            compatible = bm->Planes[p] && (UBYTE *)bm->Planes[p] ==
                         (UBYTE *)bm->Planes[0] + (size_t)p * spacing;
        if (compatible) {
            /* The upstream register ABI puts width/height/y-offset/bplsize in
             * d0/d1/d3/d5. Geometry is immutable for this screen, so the
             * self-modifying initialiser is called exactly once here. */
            c2p1x1_8_c5_030_smcinit(s->pw, s->dh, s->y0, spacing);
            s_kalms_active = 1;
        } else {
            s->use_kalms_c2p = 0;
            /* An incompatible planar allocation must never reach Kalms: use
             * the established graphics.library path instead. WPA derives its
             * source-row modulo from the visible width, so restore the normal
             * WPA padding rather than retaining Kalms' screen-wide stride.
             * Keeping the latter made HAM6 (which necessarily has only six
             * planes) read each following chunky row from the wrong offset. */
            s->pw = (dw + 15) & ~15;
            s->x0 = (sw - dw) / 2;
            if (s->x0 < 0) s->x0 = 0;
            s->x0byte = s->x0 >> 3;
            s->tempbm = AllocBitMap((ULONG)s->pw, 1, (ULONG)depth, 0, bm);
            if (!s->tempbm) goto fail;
            InitRastPort(&s->temprp);
            s->temprp.BitMap = s->tempbm;
        }
    }

    /* padded + cleared so C2P's pad columns are black */
    s->chunky = (unsigned char *)calloc((size_t)s->pw * dh, 1);
    if (!s->chunky) goto fail;
    if (scale == 2) {
        s->enc = (unsigned char *)malloc((size_t)w * h);
        if (!s->enc) goto fail;
    }
    if (resize) {
        s->scaled = (unsigned char *)malloc((size_t)dw * dh * 3);
        if (!s->scaled) goto fail;
    }
    if (!c2p && !riva_c2p_mode && !kalms_c2p_mode && !akiko) { /* WPA path */
        s->tempbm = AllocBitMap((ULONG)s->pw, 1, (ULONG)depth, 0,
                                s->scr->RastPort.BitMap);
        if (!s->tempbm) goto fail;
        InitRastPort(&s->temprp);
        s->temprp.BitMap = s->tempbm;
    }
    return s;

fail:
    s_kalms_active = 0;
    if (s->enc) free(s->enc);
    if (s->scaled) free(s->scaled);
    if (s->chunky) free(s->chunky);
    if (s->tempbm) FreeBitMap(s->tempbm);
    CloseWindow(s->win);
    CloseScreen(s->scr);
    free(s);
    return NULL;
}

static void aga_show(void *handle, const unsigned char *rgb, int w, int h,
                     int stride, int dy0, int dy1,
                     mr_display_service_fn service, void *service_opaque)
{
    aga_state *s = (aga_state *)handle;
    int pw = s->pw, dw = s->dw, sc = s->scale;
    int ddy0, ddh;
    if (!s || !s->scr) return;
    s_frame_enc = s_frame_blit = 0;

    { clock_t a = clock();
    if (s->resize) {
        /* Resize the whole frame, then encode it. Dirty rows do not map
         * cleanly through arbitrary scaling, and the fitted image is small. */
        mr_scale_resize_rgb24(rgb, w, h, stride, s->scaled,
                              dw, s->dh, dw * 3);
        uint8_t *encoded = s->chunky + (s->use_kalms_c2p ? s->x0 : 0);
        if (s->ham) mr_ham_encode(s->scaled, dw, s->dh, dw * 3, encoded, pw, s->ham);
        else if (s->depth == 5)
            dither5_rgb(s->scaled, dw, s->dh, dw * 3, encoded, pw, 0);
        else
            mr_dither_rgb8(s->scaled, dw, s->dh, dw * 3, encoded, pw, 0);
        ddy0 = 0; ddh = s->dh;
    } else {
        /* Encode only the changed source rows [dy0,dy1); the screen keeps the
         * rest. (HAM rows are independent; dither is told its y_base.) */
        const uint8_t *src;
        int rows;
        if (dy0 < 0) dy0 = 0;
        if (dy1 > h)  dy1 = h;
        if (dy1 <= dy0) {
            s_frame_enc = clock() - a; s_enc += s_frame_enc; return;
        }
        src = rgb + (size_t)dy0 * stride;
        rows = dy1 - dy0;
        if (sc == 2) {
            if (s->ham) mr_ham_encode(src, w, rows, stride, s->enc, w, s->ham);
            else if (s->depth == 5)
                dither5_rgb(src, w, rows, stride, s->enc, w, dy0);
            else
                mr_dither_rgb8(src, w, rows, stride, s->enc, w, dy0);
            mr_scale2x_u8(s->enc, w, rows, w,
                          s->chunky + (size_t)(dy0*2) * pw +
                          (s->use_kalms_c2p ? s->x0 : 0), pw);
            ddy0 = dy0 * 2; ddh = rows * 2;
        } else {
            uint8_t *dst = s->chunky + (size_t)dy0 * pw +
                           (s->use_kalms_c2p ? s->x0 : 0);
            if (s->ham) mr_ham_encode(src, w, rows, stride, dst, pw, s->ham);
            else if (s->depth == 5)
                dither5_rgb(src, w, rows, stride, dst, pw, dy0);
            else
                mr_dither_rgb8(src, w, rows, stride, dst, pw, dy0);
            ddy0 = dy0; ddh = rows;
        }
    }
    s_frame_enc = clock() - a; s_enc += s_frame_enc; }

    { clock_t a = clock();
    const uint8_t *crow = s->chunky + (size_t)ddy0 * pw;
    if (s->use_akiko) {
        struct BitMap *bm = s->scr->RastPort.BitMap;
        akiko_c2p(crow, pw, ddh, pw, s->depth,
                  (uint8_t *const *)bm->Planes, bm->BytesPerRow,
                  s->x0byte, s->y0 + ddy0);
    } else if (s->use_kalms_c2p) {
        struct BitMap *bm = s->scr->RastPort.BitMap;
        /* This routine has no row-modulo ABI. Convert the complete persistent
         * screen-width chunky frame; unchanged rows remain valid in it. */
        c2p1x1_8_c5_030(s->chunky, bm->Planes[0]);
    } else if (s->use_riva_c2p) {
        struct BitMap *bm = s->scr->RastPort.BitMap;
        mr_c2p8_riva32(crow, pw, ddh, pw, s->depth,
                       (uint8_t *const *)bm->Planes, bm->BytesPerRow,
                       s->x0byte, s->y0 + ddy0);
    } else if (s->use_c2p) {
        struct BitMap *bm = s->scr->RastPort.BitMap;
        mr_c2p8(crow, pw, ddh, pw, s->depth,
                (uint8_t *const *)bm->Planes, bm->BytesPerRow,
                s->x0byte, s->y0 + ddy0);
    } else {
        WritePixelArray8(&s->scr->RastPort,
                         (UWORD)s->x0, (UWORD)(s->y0 + ddy0),
                         (UWORD)(s->x0 + dw - 1), (UWORD)(s->y0 + ddy0 + ddh - 1),
                         (UBYTE *)crow, &s->temprp);
    }
    /* Kalms is timed and accumulated by the same counters as every other AGA
     * blit backend. Keep diagnostics out of this frame-rendering hot path. */
    s_frame_blit = clock() - a; s_blit += s_frame_blit; }
}

static int aga_poll(void *handle)
{
    aga_state *s = (aga_state *)handle;
    struct IntuiMessage *msg;
    int ev = MR_EV_NONE;
    if (!s || !s->win) return MR_EV_QUIT;
    while ((msg = (struct IntuiMessage *)GetMsg(s->win->UserPort))) {
        ULONG cls = msg->Class; UWORD code = msg->Code;
        ReplyMsg((struct Message *)msg);
        if (cls == IDCMP_RAWKEY && !(code & 0x80)) {       /* key down only  */
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

static void aga_close(void *handle)
{
    aga_state *s = (aga_state *)handle;
    if (!s) return;
    if (s->enc) free(s->enc);
    if (s->scaled) free(s->scaled);
    if (s->chunky) free(s->chunky);
    if (s->tempbm) FreeBitMap(s->tempbm);
    if (s->win) CloseWindow(s->win);
    if (s->scr) CloseScreen(s->scr);
    s_kalms_active = 0;
    free(s);
}

static ULONG aga_wait_mask(void *handle)
{
    aga_state *s = (aga_state *)handle;
    if (!s || !s->win || !s->win->UserPort) return 0;
    return 1UL << s->win->UserPort->mp_SigBit;
}

const display_backend backend_aga = {
    "AGA", aga_open, aga_show, NULL, aga_poll, aga_close, NULL, aga_wait_mask,
    NULL
};
