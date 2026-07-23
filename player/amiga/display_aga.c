/*
 * MintRIVA - AGA (planar) display backend.
 *
 * Opens a custom screen and blits each frame through a portable pixel encoder
 * (256-colour dither / HAM8 / HAM6) plus a chunky->planar step. The default
 * blit is the built-in mr_c2p8 (writes straight to the bitplanes; host-verified
 * correct), which avoids WritePixelArray8's general-case overhead; --wpa
 * selects WritePixelArray8 for comparison. Optional 2x pixel doubling.
 */
#include "amiga_display.h"
#include "display_backend.h"
#include "../core/mr_dither.h"
#include "../core/mr_ham.h"
#include "../core/mr_scale.h"
#include "../core/mr_c2p.h"

#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/gfx.h>
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
static clock_t s_enc = 0, s_blit = 0;
void display_aga_timing(unsigned long *enc_ms, unsigned long *blit_ms)
{
    if (enc_ms)  *enc_ms  = (unsigned long)(s_enc  * 1000 / CLOCKS_PER_SEC);
    if (blit_ms) *blit_ms = (unsigned long)(s_blit * 1000 / CLOCKS_PER_SEC);
}

typedef struct {
    struct Screen  *scr;
    struct Window  *win;
    struct RastPort temprp;      /* only used on the WritePixelArray8 path  */
    struct BitMap  *tempbm;
    unsigned char  *enc;         /* 1x encode buffer (scale==2 only)        */
    unsigned char  *chunky;      /* pw*dh pixels to blit                     */
    unsigned char  *scaled;      /* downscaled RGB (down>1 only)            */
    int             w, h;
    int             dw, dh;      /* displayed size                          */
    int             pw;          /* chunky row stride (>= dw)               */
    int             depth;
    int             x0, y0, x0byte;
    int             ham, scale, down, use_c2p, use_akiko;
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

static void load_palette(struct Screen *scr, int ham)
{
    ULONG tab[1 + 256 * 3 + 1];
    int   n, i;
    uint8_t pal[256 * 3];
    if (ham) { n = (ham >= 8) ? 64 : 16; mr_ham_palette(pal, ham); }
    else     { n = 256;                  mr_dither_palette(pal);   }
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
    int   c2p   = g_aga_c2p && !akiko;   /* Akiko is its own blit path */
    int   down = 1, dw, dh, depth = (ham == 6) ? 6 : 8;
    int   sw, sh;
    ULONG modeid;

    /* Fit an AGA screen (max 640 wide; 256 tall, or 512 interlaced with
     * --lace). Oversized clips are integer-downscaled to fit; small clips may
     * 2x. */
    { int maxh = g_aga_lace ? 512 : 256;
      if (w > 640 || h > maxh) {
          while (w / down > 640 || h / down > maxh) down++;
          dw = w / down; dh = h / down; scale = 1;
      } else {
          dw = w * scale; dh = h * scale;
          if (dw > 640 || dh > maxh) { scale = 1; dw = w; dh = h; }
      }
    }
    sw = (dw <= 320) ? 320 : 640;
    sh = (dh <= 256) ? 256 : ((dh + 15) & ~15);
    modeid = (dw <= 320) ? LORES_KEY : HIRES_KEY;
    if (sh > 256) modeid |= LACE;              /* interlaced when tall       */
    if (ham) modeid |= HAM;
    (void)title;

    s = (aga_state *)calloc(1, sizeof *s);
    if (!s) return NULL;
    s->w = w; s->h = h; s->dw = dw; s->dh = dh;
    s->ham = ham; s->scale = scale; s->down = down;
    s->depth = depth; s->use_c2p = c2p; s->use_akiko = akiko;
    /* Akiko converts 32 pixels per batch, so it needs a 32-pixel-aligned x and
     * a 32-multiple row stride; the built-in C2P only needs 8-pixel alignment.
     * graphics.library WritePixelArray8 requires each source row rounded up to
     * 16 pixels even though xstop names the unpadded visible width.  Packing an
     * odd width tightly makes every following row start early (854 / 2 = 427
     * exposed this as five-pixel diagonal wraps). */
    if (akiko)     { s->pw = (dw + 31) & ~31; s->x0 = ((sw - dw) / 2) & ~31; }
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
    load_palette(s->scr, ham);

    s->win = OpenWindowTags(NULL,
        WA_CustomScreen, (ULONG)s->scr,
        WA_Left, 0, WA_Top, 0, WA_Width, (ULONG)sw, WA_Height, (ULONG)sh,
        WA_Flags, WFLG_BORDERLESS | WFLG_BACKDROP | WFLG_ACTIVATE |
                  WFLG_RMBTRAP | WFLG_NOCAREREFRESH,
        WA_IDCMP, IDCMP_RAWKEY, TAG_END);
    if (!s->win) { CloseScreen(s->scr); free(s); return NULL; }

    /* padded + cleared so C2P's pad columns are black */
    s->chunky = (unsigned char *)calloc((size_t)s->pw * dh, 1);
    if (!s->chunky) goto fail;
    if (scale == 2) {
        s->enc = (unsigned char *)malloc((size_t)w * h);
        if (!s->enc) goto fail;
    }
    if (down > 1) {
        s->scaled = (unsigned char *)malloc((size_t)dw * dh * 3);
        if (!s->scaled) goto fail;
    }
    if (!c2p && !akiko) {                          /* WritePixelArray8 path   */
        s->tempbm = AllocBitMap((ULONG)s->pw, 1, (ULONG)depth, 0,
                                s->scr->RastPort.BitMap);
        if (!s->tempbm) goto fail;
        InitRastPort(&s->temprp);
        s->temprp.BitMap = s->tempbm;
    }
    return s;

fail:
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
                     int stride, int dy0, int dy1)
{
    aga_state *s = (aga_state *)handle;
    int pw = s->pw, dw = s->dw, sc = s->scale;
    int ddy0, ddh;
    if (!s || !s->scr) return;

    { clock_t a = clock();
    if (s->down > 1) {
        /* Downscale the whole frame, then encode it (full frame - dirty rows
         * don't map cleanly through a downscale, and the small image is cheap
         * to encode). */
        mr_scale_down_rgb24(rgb, w, h, stride, s->scaled, dw * 3, s->down);
        if (s->ham) mr_ham_encode(s->scaled, dw, s->dh, dw * 3, s->chunky, pw, s->ham);
        else        mr_dither_rgb8(s->scaled, dw, s->dh, dw * 3, s->chunky, pw, 0);
        ddy0 = 0; ddh = s->dh;
    } else {
        /* Encode only the changed source rows [dy0,dy1); the screen keeps the
         * rest. (HAM rows are independent; dither is told its y_base.) */
        const uint8_t *src;
        int rows;
        if (dy0 < 0) dy0 = 0;
        if (dy1 > h)  dy1 = h;
        if (dy1 <= dy0) { s_enc += clock() - a; return; }  /* nothing changed */
        src = rgb + (size_t)dy0 * stride;
        rows = dy1 - dy0;
        if (sc == 2) {
            if (s->ham) mr_ham_encode(src, w, rows, stride, s->enc, w, s->ham);
            else        mr_dither_rgb8(src, w, rows, stride, s->enc, w, dy0);
            mr_scale2x_u8(s->enc, w, rows, w, s->chunky + (size_t)(dy0*2) * pw, pw);
            ddy0 = dy0 * 2; ddh = rows * 2;
        } else {
            uint8_t *dst = s->chunky + (size_t)dy0 * pw;
            if (s->ham) mr_ham_encode(src, w, rows, stride, dst, pw, s->ham);
            else        mr_dither_rgb8(src, w, rows, stride, dst, pw, dy0);
            ddy0 = dy0; ddh = rows;
        }
    }
    s_enc += clock() - a; }

    { clock_t a = clock();
    const uint8_t *crow = s->chunky + (size_t)ddy0 * pw;
    if (s->use_akiko) {
        struct BitMap *bm = s->scr->RastPort.BitMap;
        akiko_c2p(crow, pw, ddh, pw, s->depth,
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
    s_blit += clock() - a; }
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
    free(s);
}

const display_backend backend_aga = {
    "AGA", aga_open, aga_show, aga_poll, aga_close
};
