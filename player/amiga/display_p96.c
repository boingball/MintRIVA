/*
 * MintVID - Picasso96 direct-lock RTG window backend.
 *
 * Structurally this is display_cgx.c: same Intuition window/screen open,
 * geometry cache, resize/fullscreen/poll handling. The difference is only in
 * show(): instead of WritePixelArray()'s RGB24 upload (which lets
 * cybergraphics.library convert to the screen's native depth for us), this
 * backend calls p96LockBitMap() to get a direct CPU pointer into the screen
 * bitmap and writes pixels itself - no WritePixelArray copy/convert step.
 *
 * That means the backend must already know the bitmap's native pixel layout;
 * there is no free conversion here. p96GetBitMapAttr(bm, P96BMA_RGBFORMAT)
 * is queried at open time (and rechecked whenever the underlying bitmap
 * changes) and only RGBFB_B8G8R8 (24-bit BGR - the common real Picasso96
 * truecolour mode) is supported. Anything else fails open() cleanly, and
 * display_open()'s fallback chain lands on backend_cgx instead - so
 * selecting P96 on an unsupported screen format never loses playback, only
 * the chance at the faster path. Widening to more RGBFTYPEs is a matter of
 * adding another swizzle case in write_bgr_rows()-equivalent code, not a
 * structural change.
 *
 * Per Picasso96API.library/p96LockBitMap: never hold the lock across a call
 * into other code (the doc explicitly warns against calling graphics.library
 * or Picasso96API.library functions while locked, and against holding it for
 * anywhere near a second - screen switching is disabled the whole time). So
 * the lock is taken and released per strip, with service() called only
 * between strips, never while locked - the same lesson the YUV asm's
 * register-clobbering bug taught about not assuming what happens across a
 * call into other code, applied here at the API-contract level instead of
 * the ABI level.
 *
 * Fullscreen only. A direct bitmap lock writes to raw, absolute screen
 * coordinates with none of the clipping WritePixelArray gets for free from
 * Layers: it has no idea what else is on screen, so a windowed video window
 * dragged around a normal multi-window desktop stomps whatever siblings
 * happen to be under it, with nothing to restore them afterwards (confirmed
 * on real hardware - dragging the window correctly followed the window
 * itself once win->LeftEdge/TopEdge tracking was added, but corrupted every
 * other window and icon it passed over on the way, and the desktop stayed
 * corrupted after the player closed). A private fullscreen screen has no
 * siblings to corrupt, which is the only reason direct access is safe at
 * all - see the g_display_fullscreen check in p96_open() and the matching
 * refusal in p96_toggle_fullscreen(). Properly clipping direct writes to a
 * window's actual visible region (Layer ClipRects) would lift this, but
 * that is real Amiga-internals work this project has no way to verify
 * without a compiler or real hardware in the loop at every step, so it is
 * left as a known, deliberate limitation rather than guessed at.
 */
#include "amiga_display.h"
#include "display_backend.h"
#include "mr_aspect.h"
#include "../core/mr_scale.h"

#include <stddef.h>
#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/displayinfo.h>
#include <cybergraphx/cybergraphics.h>
#include <libraries/Picasso96.h>
#include <inline/Picasso96API.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

/* Destination rows locked (and written) at a time. Kept the same size as
 * MR_CGX_STRIP_ROWS in display_cgx.c for the same reason (bounded Fast RAM
 * for the scale strip, a service() point during a large frame), plus here it
 * also bounds how long any single p96LockBitMap hold lasts. */
#define MR_P96_STRIP_ROWS 32

typedef struct {
    struct Window *win;
    struct Screen *screen;
    struct Screen *retired_screen;
    int            bl, bt;
    int            iw, ih;
    int            source_w, source_h;
    int            dx, dy, dw, dh;
    /* p96LockBitMap's RenderInfo.Memory is the whole SCREEN bitmap, not a
     * window-relative view of it the way a RastPort is - WritePixelArray
     * (display_cgx.c) draws window-relative and lets Layers/Intuition place
     * it correctly regardless of where the window sits or moves; a direct
     * lock has no such translation, so win->LeftEdge/TopEdge must be added
     * to every destination coordinate, re-read every frame since a plain
     * window drag fires no IDCMP event this backend was otherwise watching
     * for. win_x/win_y is where those were last read (see p96_rebuild_geometry);
     * last_d{x,y,w,h}/have_last_rect is the exact rect last drawn there, kept
     * so a detected move can black out that old absolute position first -
     * WFLG_NOCAREREFRESH (matching display_cgx.c) means Intuition will not
     * clean it up on our behalf, and nothing else owns that screen memory. */
    int            win_x, win_y;
    int            last_dx, last_dy, last_dw, last_dh, have_last_rect;
    int            pending_w, pending_h;
    clock_t        resize_at;
    unsigned char *scaled;   /* persistent RGB24 scale strip, swizzled to BGR
                               * just before the locked write - see show()   */
    size_t         scaled_size;
    int            scaled_w, scaled_stride;
    int            cached_dst_w;
    int            cached_dst_h;
    struct BitMap *cached_bitmap;
    int            geometry_valid;
    int            format_ok;   /* live bitmap is still RGBFB_B8G8R8/24bpp   */
    mr_display_timing timing;
    int            quit;
    int            fullscreen;
    int            have_window_geometry;
    int            window_left, window_top, window_width, window_height;
    int            window_iw, window_ih;
    int            force_full_redraw;
    const char    *title;
} p96_state;

static void p96_rebuild_geometry(p96_state *s, const char *reason);
static int  ensure_scaled(p96_state *s, int w);
static int  p96_toggle_fullscreen(void *h);
static int  bitmap_format_ok(struct BitMap *bm);
static void clear_bgr24_rect(struct BitMap *bm, int x, int y, int w, int h);

static int p96_close_private_screen(struct Screen **screen, const char *reason)
{
    int attempt;
    if (!screen || !*screen) return 1;

    WaitBlit();
    for (attempt = 0; attempt < 50; attempt++) {
        if (CloseScreen(*screen)) {
            *screen = NULL;
            if (attempt > 0)
                printf("p96-fullscreen: private screen closed after %d "
                       "VBlank(s) (%s)\n", attempt,
                       reason ? reason : "unknown");
            return 1;
        }
        WaitTOF();
    }
    ScreenToBack(*screen);
    printf("p96-fullscreen: WARNING private screen still busy after "
           "50 VBlanks (%s)\n", reason ? reason : "unknown");
    return 0;
}

static struct Screen *p96_open_private_screen(p96_state *s, const char *title)
{
    /* Unlike display_cgx.c (which picks the lowest depth that still shows the
     * picture, since WritePixelArray converts for it), this backend only
     * knows how to write fixed 24-bit BGR - a private screen opened at any
     * other depth would make bitmap_format_ok() correctly refuse to draw,
     * dropping every frame (audio keeps running, screen just goes black).
     * That's exactly what happened before this fix: a card offering 16-bit
     * modes had them picked first. Only 24-bit is a candidate here. */
    static const ULONG depths[] = { 24 };
    struct Screen *public_screen;
    struct Screen *scr = NULL;
    ULONG best_modeid = (ULONG)INVALID_ID;
    ULONG best_depth = 0;
    ULONG best_score = ~0UL;
    int target_w = s->source_w;
    int target_h = s->source_h;
    unsigned i;

    public_screen = LockPubScreen(NULL);
    if (public_screen) {
        if (public_screen->Width > 0 && public_screen->Height > 0) {
            target_w = public_screen->Width;
            target_h = public_screen->Height;
        }
        UnlockPubScreen(NULL, public_screen);
    }

    for (i = 0; i < sizeof depths / sizeof depths[0]; i++) {
        ULONG modeid = BestCModeIDTags(
            CYBRBIDTG_NominalWidth, (ULONG)target_w,
            CYBRBIDTG_NominalHeight, (ULONG)target_h,
            CYBRBIDTG_Depth, depths[i],
            TAG_END);
        ULONG mode_w, mode_h, mode_depth, score;
        if (modeid == (ULONG)INVALID_ID || !IsCyberModeID(modeid))
            continue;
        mode_w = GetCyberIDAttr(CYBRIDATTR_WIDTH, modeid);
        mode_h = GetCyberIDAttr(CYBRIDATTR_HEIGHT, modeid);
        mode_depth = GetCyberIDAttr(CYBRIDATTR_DEPTH, modeid);
        if (!mode_w || !mode_h || mode_w == ~0UL || mode_h == ~0UL ||
            mode_depth <= 8 || mode_depth == ~0UL)
            continue;
        score = (ULONG)((int)mode_w >= target_w ? (int)mode_w - target_w
                                                     : target_w - (int)mode_w) +
                (ULONG)((int)mode_h >= target_h ? (int)mode_h - target_h
                                                     : target_h - (int)mode_h);
        if (score < best_score ||
            (score == best_score && mode_depth < best_depth)) {
            best_modeid = modeid;
            best_depth = mode_depth;
            best_score = score;
        }
    }
    if (best_modeid != (ULONG)INVALID_ID)
        scr = OpenScreenTags(NULL,
            SA_DisplayID, best_modeid,
            SA_Depth, best_depth,
            SA_Type, CUSTOMSCREEN,
            SA_Title, (ULONG)(title ? title : "MintVID"),
            SA_Quiet, TRUE,
            SA_ShowTitle, FALSE,
            SA_Draggable, FALSE,
            TAG_END);
    /* Depth alone (checked below via CYBRMATTR_DEPTH) doesn't guarantee the
     * exact RGBFB_B8G8R8 layout this backend writes - verify the real
     * format on the screen we actually got, the same check open()/
     * p96_rebuild_geometry() rely on, rather than trusting "24-bit implies
     * BGR". */
    if (scr && (GetCyberMapAttr(scr->RastPort.BitMap, CYBRMATTR_DEPTH) <= 8 ||
                !bitmap_format_ok(scr->RastPort.BitMap))) {
        CloseScreen(scr);
        scr = NULL;
    }
    if (scr && g_display_want_time)
        printf("p96-fullscreen private=%dx%d depth=%lu target=%dx%d "
               "source=%dx%d\n",
               scr->Width, scr->Height,
               (unsigned long)GetCyberMapAttr(scr->RastPort.BitMap,
                                               CYBRMATTR_DEPTH),
               target_w, target_h, s->source_w, s->source_h);
    return scr;
}

static struct Window *p96_open_window(p96_state *s, const char *title,
                                      int inner_w, int inner_h,
                                      struct Screen **private_screen)
{
    struct Screen *scr;
    struct Window *win = NULL;
    if (private_screen) *private_screen = NULL;
    if (s->fullscreen) {
        scr = p96_open_private_screen(s, title);
        if (scr) {
            win = OpenWindowTags(NULL,
                WA_CustomScreen, (ULONG)scr,
                WA_Title, (ULONG)(title ? title : "MintVID"),
                WA_Left, 0, WA_Top, 0,
                WA_Width, (ULONG)scr->Width, WA_Height, (ULONG)scr->Height,
                WA_Flags, WFLG_BORDERLESS | WFLG_BACKDROP | WFLG_ACTIVATE |
                          WFLG_RMBTRAP | WFLG_NOCAREREFRESH,
                WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_RAWKEY,
                TAG_END);
            if (win) {
                if (private_screen) *private_screen = scr;
                return win;
            }
            CloseScreen(scr);
        }
        if (g_display_want_time)
            printf("p96-fullscreen: private screen unavailable, "
                   "using Workbench screen\n");
    }

    scr = LockPubScreen(NULL);
    if (!scr) return NULL;
    if (s->fullscreen) {
        win = OpenWindowTags(NULL,
            WA_PubScreen, (ULONG)scr,
            WA_Title, (ULONG)(title ? title : "MintVID"),
            WA_Left, 0, WA_Top, 0,
            WA_Width, (ULONG)scr->Width, WA_Height, (ULONG)scr->Height,
            WA_Borderless, TRUE, WA_Activate, TRUE,
            WA_Flags, WFLG_NOCAREREFRESH | WFLG_RMBTRAP,
            WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_RAWKEY | IDCMP_NEWSIZE,
            TAG_END);
    } else {
        win = OpenWindowTags(NULL,
            WA_PubScreen, (ULONG)scr,
            WA_Title, (ULONG)(title ? title : "MintVID"),
            WA_InnerWidth, (ULONG)inner_w,
            WA_InnerHeight, (ULONG)inner_h,
            WA_Flags, WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_CLOSEGADGET |
                      WFLG_SIZEGADGET | WFLG_ACTIVATE | WFLG_NOCAREREFRESH,
            WA_MinWidth, 160, WA_MinHeight, 100,
            WA_MaxWidth, (ULONG)-1, WA_MaxHeight, (ULONG)-1,
            WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_RAWKEY | IDCMP_NEWSIZE,
            TAG_END);
    }
    UnlockPubScreen(NULL, scr);
    return win;
}

static int p96_default_screen_is_rtg(void)
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

/* Only RGBFB_B8G8R8 (packed 24-bit BGR, 3 bytes/pixel) is supported today -
 * see the file header comment. bm may be any live BitMap (public screen or a
 * private one this backend just opened). */
static int bitmap_format_ok(struct BitMap *bm)
{
    ULONG format, bpp;
    if (!bm || !P96Base) return 0;
    format = p96GetBitMapAttr(bm, P96BMA_RGBFORMAT);
    bpp = p96GetBitMapAttr(bm, P96BMA_BYTESPERPIXEL);
    return format == (ULONG)RGBFB_B8G8R8 && bpp == 3;
}

static void fit_within(int w, int h, int max_w, int max_h, int *out_w,
                       int *out_h)
{
    *out_w = w;
    *out_h = h;
    if (w <= 0 || h <= 0 || max_w <= 0 || max_h <= 0)
        return;
    if (w <= max_w && h <= max_h)
        return;
    if ((long)w * max_h > (long)h * max_w) {
        *out_w = max_w;
        *out_h = (int)(((long)h * max_w + w / 2) / w);
    } else {
        *out_h = max_h;
        *out_w = (int)(((long)w * max_h + h / 2) / h);
    }
    if (*out_w < 1) *out_w = 1;
    if (*out_h < 1) *out_h = 1;
}

static void *p96_open(int w, int h, const char *title)
{
    p96_state *s;
    int win_w = w, win_h = h;
    struct Screen *scr;
    if (!CyberGfxBase || !P96Base || !p96_default_screen_is_rtg())
        return NULL;                              /* not RTG -> try CGX/AGA */

    /* Fullscreen only - see the file header comment. A direct bitmap lock
     * has no equivalent of the window-clipping WritePixelArray gets for free
     * from Layers: every write lands at raw screen coordinates regardless of
     * what else is on screen, so a windowed video window dragged around a
     * normal desktop corrupts whatever sibling windows/icons happen to be
     * under it - a private fullscreen screen has no siblings to corrupt,
     * which is the only reason this is safe at all. Fall back to CGX/AGA for
     * anything windowed rather than risk it. */
    if (!g_display_fullscreen) {
        printf("p96: windowed mode is not supported (direct bitmap locks "
               "have no window clipping and would corrupt other windows on "
               "the desktop) - falling back to WritePixelArray; use "
               "--fullscreen to use P96\n");
        return NULL;
    }

    s = (p96_state *)AllocVec(sizeof *s, MEMF_CLEAR);
    if (!s) return NULL;

    scr = LockPubScreen(NULL);
    if (scr) {
        int avail_w = scr->Width - scr->WBorLeft - scr->WBorRight;
        int avail_h = scr->Height - scr->BarHeight - 1 - scr->WBorTop -
                      scr->WBorBottom;
        UnlockPubScreen(NULL, scr);
        if (avail_w < 160) avail_w = 160;
        if (avail_h < 100) avail_h = 100;
        fit_within(w, h, avail_w, avail_h, &win_w, &win_h);
    }

    s->fullscreen = g_display_fullscreen;
    s->source_w = w;
    s->source_h = h;
    s->title = title;
    s->win = p96_open_window(s, title, win_w, win_h, &s->screen);
    if (!s->win) { FreeVec(s); return NULL; }

    if (!bitmap_format_ok(s->win->RPort->BitMap)) {
        if (g_display_want_time)
            printf("p96: screen bitmap format is not 24-bit BGR "
                   "(RGBFB_B8G8R8) - falling back to WritePixelArray\n");
        CloseWindow(s->win);
        p96_close_private_screen(&s->screen, "unsupported format");
        FreeVec(s);
        return NULL;
    }

    s->bl = s->win->BorderLeft;
    s->bt = s->win->BorderTop;
    s->iw = s->win->Width - s->win->BorderLeft - s->win->BorderRight;
    s->ih = s->win->Height - s->win->BorderTop - s->win->BorderBottom;
    s->pending_w = s->iw; s->pending_h = s->ih;
    s->force_full_redraw = 1;
    if (!s->fullscreen) {
        s->have_window_geometry = 1;
        s->window_left = s->win->LeftEdge;
        s->window_top = s->win->TopEdge;
        s->window_width = s->win->Width;
        s->window_height = s->win->Height;
        s->window_iw = s->iw;
        s->window_ih = s->ih;
    }

    s->cached_dst_w  = -1;
    s->cached_dst_h  = -1;
    s->cached_bitmap = NULL;
    s->geometry_valid = 0;
    s->format_ok = 1;

    p96_rebuild_geometry(s, "init");

    return s;
}

static unsigned long elapsed_us(clock_t begin)
{
    return (unsigned long)((clock() - begin) * 1000000UL / CLOCKS_PER_SEC);
}

static void p96_rebuild_geometry(p96_state *s, const char *reason)
{
    struct BitMap *bm;
    int bm_changed, scale_map_changed, old_iw, old_ih;
    int win_x, win_y, position_changed;
    mr_aspect_rect fit;
    ULONG bm_depth = 0;

    old_iw = s->iw;
    old_ih = s->ih;
    s->bl = s->win->BorderLeft;
    s->bt = s->win->BorderTop;
    s->iw = s->win->Width  - s->win->BorderLeft - s->win->BorderRight;
    s->ih = s->win->Height - s->win->BorderTop  - s->win->BorderBottom;
    if (s->iw < 1) s->iw = 1;
    if (s->ih < 1) s->ih = 1;

    fit = mr_aspect_fit(s->source_w, s->source_h, s->iw, s->ih);

    scale_map_changed = (fit.w != s->cached_dst_w ||
                         fit.h != s->cached_dst_h ||
                         fit.x != s->dx || fit.y != s->dy ||
                         s->iw != old_iw || s->ih != old_ih);

    bm = s->win->RPort->BitMap;
    bm_changed = (bm != s->cached_bitmap);
    if (bm && CyberGfxBase)
        bm_depth = GetCyberMapAttr(bm, CYBRMATTR_DEPTH);
    if (bm_changed)
        s->format_ok = bitmap_format_ok(bm);

    win_x = s->win->LeftEdge;
    win_y = s->win->TopEdge;
    position_changed = s->geometry_valid && !bm_changed &&
                       (win_x != s->win_x || win_y != s->win_y);
    /* Erase the old absolute rect before anything draws at the new one -
     * same bitmap, same format (bm_changed is false here), so the lock this
     * needs is exactly the one write_bgr24_strip already relies on. A
     * genuine screen/bitmap change (bm_changed) needs no such cleanup: it's
     * either a different physical bitmap the old rect doesn't apply to, or
     * the FillPixelArray below already clears the new window's own area. */
    if (position_changed && s->format_ok && s->have_last_rect)
        clear_bgr24_rect(bm, s->win_x + s->bl + s->last_dx,
                         s->win_y + s->bt + s->last_dy,
                         s->last_dw, s->last_dh);
    s->win_x = win_x;
    s->win_y = win_y;

    s->dx = fit.x; s->dy = fit.y;
    s->dw = fit.w; s->dh = fit.h;
    s->cached_dst_w  = s->dw;
    s->cached_dst_h  = s->dh;
    s->cached_bitmap = bm;
    s->geometry_valid = 1;
    if (scale_map_changed || bm_changed) {
        FillPixelArray(s->win->RPort, (UWORD)s->bl, (UWORD)s->bt,
                       (UWORD)s->iw, (UWORD)s->ih, 0x00000000UL);
        s->force_full_redraw = 1;
    }
    /* Only the delta rows since last frame get redrawn otherwise (the caller
     * tracks source-frame changes, not screen-position ones) - after a pure
     * move the whole picture needs repainting at the new absolute position,
     * not just whatever rows happened to change in the video itself. */
    if (position_changed)
        s->force_full_redraw = 1;
    s->last_dx = s->dx; s->last_dy = s->dy;
    s->last_dw = s->dw; s->last_dh = s->dh;
    s->have_last_rect = 1;

    if (g_display_want_time)
        printf("p96-geometry reason=%s iw=%d ih=%d video=%d,%d %dx%d "
               "bm-changed=%d scale-map-rebuild=%d position-changed=%d "
               "depth=%lu format-ok=%d\n",
               reason ? reason : "?",
               s->iw, s->ih, s->dx, s->dy, s->dw, s->dh,
               bm_changed, scale_map_changed, position_changed,
               (unsigned long)bm_depth, s->format_ok);
}

static void report_slow(const char *operation, unsigned long usec,
                        mr_display_service_fn service, void *opaque)
{
    if (usec <= 10000) return;
    if (service) service(opaque);
    printf("p96-slow operation=%s duration=%lu us\n", operation, usec);
    if (service) service(opaque);
}

static int ensure_scaled(p96_state *s, int w)
{
    size_t stride, bytes;
    unsigned char *p;
    if (w <= 0 || w > 65535) return 0;
    if ((size_t)w > (size_t)-1 / 3) return 0;
    stride = (size_t)w * 3;
    if ((size_t)MR_P96_STRIP_ROWS > (size_t)-1 / stride) return 0;
    bytes = stride * (size_t)MR_P96_STRIP_ROWS;
    if (s->scaled && s->scaled_w == w) return 1;
    p = (unsigned char *)AllocVec(bytes, MEMF_ANY);
    if (!p) return 0;
    if (s->scaled) FreeVec(s->scaled);
    s->scaled = p; s->scaled_size = bytes;
    s->scaled_w = w; s->scaled_stride = (int)stride;
    return 1;
}

static void scale_rgb24_strip(p96_state *s, const unsigned char *src, int sw,
                              int sh, int src_stride, int dst_y0, int rows)
{
    mr_scale_resize_rgb24_strip(src, sw, sh, src_stride, s->scaled, s->dw,
                                s->dh, s->scaled_stride, dst_y0, rows);
}

/* Black out a rect directly (same lock/unlock discipline as
 * write_bgr24_strip). Used to erase the last-drawn video rect at its OLD
 * absolute screen position when the window has moved - see the p96_state
 * comment on win_x/win_y for why that cleanup is this backend's own job. */
static void clear_bgr24_rect(struct BitMap *bm, int x, int y, int w, int h)
{
    struct RenderInfo ri;
    LONG lock;
    int row, bpr;
    unsigned char *base;

    if (w <= 0 || h <= 0) return;
    lock = p96LockBitMap(bm, (UBYTE *)&ri, sizeof ri);
    if (!lock) return;
    bpr = (int)ri.BytesPerRow;
    base = (unsigned char *)ri.Memory + (size_t)y * (size_t)bpr + (size_t)x * 3;
    for (row = 0; row < h; row++)
        memset(base + (size_t)row * (size_t)bpr, 0, (size_t)w * 3);
    p96UnlockBitMap(bm, lock);
}

/* Lock the live bitmap, write `rows` of RGB24 `src` (R,G,B byte order - the
 * player's universal internal format) into it starting at (dst_x, dst_y) as
 * BGR24, unlock. Never holds the lock across a service() call - see the file
 * header comment. Returns 0 (and leaves the frame undrawn for this strip) if
 * the lock could not be taken; the caller treats that like any other dropped
 * strip, not a fatal error. */
static int write_bgr24_strip(struct BitMap *bm, int dst_x, int dst_y,
                             const unsigned char *src, int src_stride,
                             int w, int rows)
{
    struct RenderInfo ri;
    LONG lock;
    int y;
    unsigned char *base;
    int bpr;

    lock = p96LockBitMap(bm, (UBYTE *)&ri, sizeof ri);
    if (!lock) return 0;

    bpr = (int)ri.BytesPerRow;
    base = (unsigned char *)ri.Memory + (size_t)dst_y * (size_t)bpr +
           (size_t)dst_x * 3;
    for (y = 0; y < rows; y++) {
        const unsigned char *srow = src + (size_t)y * (size_t)src_stride;
        unsigned char *drow = base + (size_t)y * (size_t)bpr;
        int x;
        for (x = 0; x < w; x++) {
            drow[x * 3 + 0] = srow[x * 3 + 2]; /* B */
            drow[x * 3 + 1] = srow[x * 3 + 1]; /* G */
            drow[x * 3 + 2] = srow[x * 3 + 0]; /* R */
        }
    }

    p96UnlockBitMap(bm, lock);
    return 1;
}

static void p96_show(void *h, const unsigned char *rgb, int w, int hh,
                     int stride, int dy0, int dy1,
                     mr_display_service_fn service, void *service_opaque)
{
    p96_state *s = (p96_state *)h;
    clock_t total = 0, mark = 0;
    int native, timing;
    struct BitMap *bm;
    if (!s || !s->win) return;
    timing = g_display_want_time;
    if (timing) {
        memset(&s->timing, 0, sizeof s->timing);
        total = mark = clock();
    }
    {
        struct BitMap *cur_bm = s->win->RPort->BitMap;
        int moved = s->win->LeftEdge != s->win_x || s->win->TopEdge != s->win_y;
        int need_rebuild = !s->geometry_valid || cur_bm != s->cached_bitmap ||
                          moved;
        if (!need_rebuild &&
            (s->pending_w != s->iw || s->pending_h != s->ih) &&
            clock() - s->resize_at >= CLOCKS_PER_SEC / 10)
            need_rebuild = 1;
        if (need_rebuild)
            p96_rebuild_geometry(s, !s->geometry_valid ? "invalid" :
                                 cur_bm != s->cached_bitmap ? "bitmap-change" :
                                 moved ? "moved" : "resize");
    }
    if (timing) {
        s->timing.resize_us = elapsed_us(mark);
        report_slow("resize-handling", s->timing.resize_us,
                    service, service_opaque);
    }

    if (!s->format_ok) {
        /* The live bitmap stopped being 24-bit BGR (e.g. a screen-mode
         * change moved us to a different depth/format). There is no way to
         * draw correctly here; rather than write garbage, drop frames and
         * say so once per report_slow-style throttle. The caller (mrplay)
         * still owns falling back to a different --display mode; this
         * backend cannot switch itself mid-playback. */
        printf("p96-error: live bitmap is no longer 24-bit BGR - dropping "
               "frame (relaunch with --display cgx)\n");
        return;
    }

    if (timing) mark = clock();
    if (s->force_full_redraw) {
        dy0 = 0;
        dy1 = hh;
        s->force_full_redraw = 0;
    }
    if (dy0 < 0) dy0 = 0;
    if (dy1 > hh) dy1 = hh;
    if (dy1 <= dy0) return;
    if (timing) {
        s->timing.clip_us = elapsed_us(mark);
        report_slow("clipping-setup", s->timing.clip_us,
                    service, service_opaque);
        mark = clock();
    }
    if (timing) {
        s->timing.src_w = w; s->timing.src_h = hh;
        s->timing.dst_w = s->dw; s->timing.dst_h = s->dh;
        s->timing.src_format = "RGB24";
        s->timing.dst_format = "BGR24";
    }
    native = s->dw == w && s->dh == hh;
    if (timing) {
        s->timing.geometry_us = elapsed_us(mark);
        report_slow("geometry-format-setup", s->timing.geometry_us,
                    service, service_opaque);
    }

    bm = s->win->RPort->BitMap;

    if (!native) {
        int y;
        if (timing) mark = clock();
        if (service) service(service_opaque);
        if (!ensure_scaled(s, s->dw)) {
            if (service) service(service_opaque);
            printf("p96-error: scale strip allocation failed (%dx%d, "
                   "%lu bytes) - dropping frame\n",
                   s->dw, MR_P96_STRIP_ROWS,
                   (unsigned long)((size_t)s->dw * 3 *
                                   (size_t)MR_P96_STRIP_ROWS));
            return;
        }
        if (service) service(service_opaque);
        if (timing) {
            s->timing.allocation_us = elapsed_us(mark);
            report_slow("destination-buffer-allocation",
                        s->timing.allocation_us, service, service_opaque);
        }

        if (timing) s->timing.prepare_us = elapsed_us(total);
        if (service) service(service_opaque);

        if (timing) {
            s->timing.scale_us = 0;
            s->timing.blit_us = 0;
        }
        for (y = 0; y < s->dh; y += MR_P96_STRIP_ROWS) {
            int rows = s->dh - y < MR_P96_STRIP_ROWS ? s->dh - y
                                                      : MR_P96_STRIP_ROWS;
            clock_t step;

            if (timing) step = clock();
            scale_rgb24_strip(s, rgb, w, hh, stride, y, rows);
            if (timing) s->timing.scale_us += elapsed_us(step);

            if (timing) step = clock();
            if (!write_bgr24_strip(bm, s->win_x + s->bl + s->dx,
                                   s->win_y + s->bt + s->dy + y,
                                   s->scaled, s->scaled_stride, s->dw, rows))
                printf("p96-error: p96LockBitMap failed - dropped strip\n");
            if (timing) s->timing.blit_us += elapsed_us(step);

            if (service) service(service_opaque);
        }
        if (timing) {
            report_slow("software-scale", s->timing.scale_us,
                        service, service_opaque);
            report_slow("p96-lock-write", s->timing.blit_us,
                        service, service_opaque);
        }
        if (timing) {
            s->timing.pixels = (unsigned long)s->dw * (unsigned long)s->dh;
            s->timing.bytes = (unsigned long)s->scaled_size *
                              (unsigned long)((s->dh + MR_P96_STRIP_ROWS - 1) /
                                              MR_P96_STRIP_ROWS);
            s->timing.copies =
                (unsigned long)((s->dh + MR_P96_STRIP_ROWS - 1) /
                                MR_P96_STRIP_ROWS);
        }
        if (service) service(service_opaque);
        if (timing) s->timing.total_us = elapsed_us(total);
        return;
    }

    if (timing) s->timing.prepare_us = elapsed_us(total);
    if (service) service(service_opaque);
    if (timing) mark = clock();
    {
        int y;
        for (y = dy0; y < dy1; y += MR_P96_STRIP_ROWS) {
            int rows = dy1 - y < MR_P96_STRIP_ROWS ? dy1 - y
                                                    : MR_P96_STRIP_ROWS;
            if (!write_bgr24_strip(bm, s->win_x + s->bl + s->dx,
                                   s->win_y + s->bt + s->dy + y,
                                   rgb + (size_t)y * stride, stride, w, rows))
                printf("p96-error: p96LockBitMap failed - dropped strip\n");
            if (timing) s->timing.copies++;
            if (service) service(service_opaque);
        }
    }
    if (timing) s->timing.blit_us = elapsed_us(mark);
    if (timing) {
        s->timing.pixels = (unsigned long)w * (unsigned long)(dy1 - dy0);
        s->timing.bytes = (unsigned long)stride * (unsigned long)(dy1 - dy0);
        s->timing.total_us = elapsed_us(total);
    }
}

static int p96_timing(void *h, mr_display_timing *timing)
{
    p96_state *s = (p96_state *)h;
    if (!s || !timing) return 0;
    *timing = s->timing; return 1;
}

static int p96_poll(void *h)
{
    p96_state *s = (p96_state *)h;
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
        else if (cls == IDCMP_RAWKEY && !(code & 0x80)) {
            switch (code) {
            case 0x45: s->quit = 1; break;
            case 0x40: ev = MR_EV_PAUSE; break;
            case 0x23: p96_toggle_fullscreen(s); break;
            case 0x4E: ev = MR_EV_SEEK_FWD; break;
            case 0x4F: ev = MR_EV_SEEK_BACK; break;
            }
        }
    }
    return s->quit ? MR_EV_QUIT : ev;
}

static int p96_toggle_fullscreen(void *h)
{
    p96_state *s = (p96_state *)h;
    struct Window *old, *replacement;
    struct Screen *old_screen, *replacement_screen = NULL;
    struct IntuiMessage *msg;
    int next_fullscreen;
    if (!s || !s->win) return 0;
    if (s->fullscreen) {
        /* Refuse to leave fullscreen - see p96_open()'s comment. Windowed
         * mode on a shared desktop is unsafe for this backend's direct,
         * unclipped bitmap writes; the initial open() already only ever
         * succeeds in fullscreen, so honour that for the lifetime of the
         * window too rather than let F reintroduce the same corruption. */
        printf("p96: cannot leave fullscreen (windowed mode unsupported - "
               "see --p96 above); use --display cgx for a windowed RTG "
               "session instead\n");
        return 0;
    }
    p96_close_private_screen(&s->retired_screen, "toggle retry");
    if (!s->fullscreen && s->retired_screen) {
        printf("p96-fullscreen: previous private screen still busy\n");
        return 0;
    }
    old = s->win;
    old_screen = s->screen;
    if (!s->fullscreen) {
        s->have_window_geometry = 1;
        s->window_left = old->LeftEdge;
        s->window_top = old->TopEdge;
        s->window_width = old->Width;
        s->window_height = old->Height;
        s->window_iw = old->Width - old->BorderLeft - old->BorderRight;
        s->window_ih = old->Height - old->BorderTop - old->BorderBottom;
    }
    next_fullscreen = !s->fullscreen;
    s->fullscreen = next_fullscreen;
    replacement = p96_open_window(s, s->title,
        s->have_window_geometry ? s->window_iw : s->iw,
        s->have_window_geometry ? s->window_ih : s->ih,
        &replacement_screen);
    if (!replacement) {
        s->fullscreen = !next_fullscreen;
        return 0;
    }
    while ((msg = (struct IntuiMessage *)GetMsg(old->UserPort)) != NULL)
        ReplyMsg((struct Message *)msg);
    CloseWindow(old);
    s->win = replacement;
    s->screen = replacement_screen;
    if (old_screen &&
        !p96_close_private_screen(&old_screen, "fullscreen toggle"))
        s->retired_screen = old_screen;
    if (!s->fullscreen && s->have_window_geometry)
        ChangeWindowBox(s->win, s->window_left, s->window_top,
                        s->window_width, s->window_height);
    s->bl = s->win->BorderLeft;
    s->bt = s->win->BorderTop;
    s->pending_w = s->iw = s->win->Width - s->win->BorderLeft -
                            s->win->BorderRight;
    s->pending_h = s->ih = s->win->Height - s->win->BorderTop -
                            s->win->BorderBottom;
    s->cached_dst_w = s->cached_dst_h = -1;
    s->cached_bitmap = NULL;
    s->geometry_valid = 0;
    s->force_full_redraw = 1;
    return 1;
}

static void p96_status(void *h, const char *text)
{
    p96_state *s = (p96_state *)h;
    const char *title = (text && *text) ? text : "MintVID";
    if (!s || !s->win) return;
    if (s->title == title) return;
    s->title = title;
    SetWindowTitles(s->win, (CONST_STRPTR)title, (CONST_STRPTR)~0UL);
}

static void p96_close(void *h)
{
    p96_state *s = (p96_state *)h;
    struct IntuiMessage *msg;

    if (!s) return;

    if (s->win) {
        ModifyIDCMP(s->win, 0);
        if (s->win->UserPort) {
            while ((msg = (struct IntuiMessage *)GetMsg(s->win->UserPort)))
                ReplyMsg((struct Message *)msg);
        }
        /*
         * p96LockBitMap is released per strip, but wait for any graphics-side
         * work to settle before dropping the fullscreen window/screen.
         */
        WaitBlit();
        CloseWindow(s->win);
        s->win = NULL;
        WaitTOF();
        WaitTOF();
    }

    p96_close_private_screen(&s->screen, "shutdown");
    p96_close_private_screen(&s->retired_screen, "shutdown retry");

    if (s->scaled) FreeVec(s->scaled);
    FreeVec(s);
}

static ULONG p96_wait_mask(void *h)
{
    p96_state *s = (p96_state *)h;
    if (!s || !s->win || !s->win->UserPort) return 0;
    return 1UL << s->win->UserPort->mp_SigBit;
}

const display_backend backend_p96 = {
    .name = "RTG (P96)",
    .open = p96_open,
    .show = p96_show,
    .timing = p96_timing,
    .poll = p96_poll,
    .close = p96_close,
    .status = p96_status,
    .wait_mask = p96_wait_mask,
    .toggle_fullscreen = p96_toggle_fullscreen
    /* supports_indexed/show_indexed/supports_yuv_indexed left NULL - RTG
     * backends don't implement the AGA-only indexed fast paths. */
};
