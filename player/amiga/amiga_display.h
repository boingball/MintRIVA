/*
 * MintRIVA - Amiga display backend (abstract).
 *
 * The player talks to this, not to Intuition/cybergraphics directly, so a
 * faster fullscreen RTG path or an AGA C2P path can slot in later behind the
 * same three calls without touching the player loop.
 *
 * The first backend (display_cgx.c) opens an RTG window on the default public
 * screen and blits RGB24 frames with cybergraphics.library WritePixelArray -
 * correct and simple; optimisation (direct RGB565, fullscreen, RiVA's blitters)
 * comes later.
 */
#ifndef AMIGA_DISPLAY_H
#define AMIGA_DISPLAY_H

typedef struct amiga_display amiga_display;
typedef void (*mr_display_service_fn)(void *opaque);

typedef struct mr_display_timing {
    unsigned long prepare_us, scale_us, convert_us, copy_us, blit_us;
    unsigned long clip_us, total_us, pixels, bytes;
    unsigned long geometry_us, resize_us, allocation_us, setup_us;
    unsigned int src_w, src_h, dst_w, dst_h, copies;
    const char *src_format, *dst_format;
} mr_display_timing;

/* Force the AGA backend (skip the RTG/cybergraphics attempt). Call before
 * display_open; default is RTG-first with automatic AGA fallback. */
void display_set_force_aga(int on);

/* AGA colour mode: 0 = 256-colour dither (default), 6 = HAM6, 8 = HAM8.
 * A non-zero HAM depth implies the AGA backend (HAM is AGA-only). */
void display_set_ham(int bits);

/* Integer upscale for the AGA backend: 1 (default) or 2. */
void display_set_scale(int n);

/* AGA blit path: 0 = graphics WritePixelArray8 (default), 1 = built-in C2P. */
void display_set_c2p(int on);

/* Select the experimental RiVA-inspired, 32-pixel direct-to-plane C2P.  It is
 * deliberately opt-in until it has been benchmarked on each 68k generation. */
void display_set_riva_c2p(int on);

/* Select Kalms' public-domain c2p1x1_8_c5_030 AGA converter. */
void display_set_kalms_c2p(int on);

/* Allow interlaced AGA screens (up to ~640x512). The AGA fitter compensates
 * for the doubled vertical resolution, preserving the video's physical aspect
 * ratio. Off by default because interlace flickers on native displays. */
void display_set_lace(int on);

/* Use the CD32 Akiko chip's hardware chunky->planar instead of the CPU C2P.
 * CD32 only; no effect (and unsafe) elsewhere, so gate it on --cd32. */
void display_set_akiko(int on);

/* Enable timing/diagnostic output for the RTG backend (mirrors --time).
 * Must be called before display_open() to capture init diagnostics. */
void display_set_timing_mode(int on);
void display_set_fullscreen(int on);

/* Accumulated AGA encode / blit time in ms (0 if the AGA backend wasn't used). */
void display_aga_timing(unsigned long *enc_ms, unsigned long *blit_ms);

/* Most recent AGA frame's conversion and blit times (rather than totals). */
void display_aga_frame_timing(unsigned long *enc_ms, unsigned long *blit_ms);

/* Return non-zero when the opened AGA screen is using Kalms, optionally
 * returning its accumulated conversion time. */
int display_aga_kalms_timing(unsigned long *conversion_ms);

/* Open a display able to show w*h frames: tries RTG (cybergraphics) first, then
 * falls back to AGA. Returns NULL only if neither works. */
amiga_display *display_open(int w, int h, const char *title);

/* Name of the backend that actually opened ("RTG (CGX)" / "AGA"). */
const char *display_backend_name(amiga_display *d);

/* Blit one RGB24 frame (r,g,b bytes, `stride` bytes per row, top-down). Only
 * source rows [dy0,dy1) are redrawn; pass 0..h to draw the whole frame. */
void display_show_rgb(amiga_display *d, const unsigned char *rgb,
                      int w, int h, int stride, int dy0, int dy1);
void display_set_service(amiga_display *d, mr_display_service_fn fn,
                         void *opaque);
int display_rtg_frame_timing(amiga_display *d, mr_display_timing *timing);
int display_toggle_fullscreen(amiga_display *d);

/* Input events reported by display_poll_event. */
enum {
    MR_EV_NONE = 0,
    MR_EV_QUIT,          /* ESC or close gadget                            */
    MR_EV_PAUSE,         /* space - toggle pause                           */
    MR_EV_SEEK_FWD,      /* cursor right                                   */
    MR_EV_SEEK_BACK      /* cursor left                                    */
};

/* Non-blocking: returns the most significant queued input event (QUIT wins). */
int  display_poll_event(amiga_display *d);
unsigned long display_wait_mask(amiga_display *d);

/* Show a short status string (e.g. "Buffering...", "Reconnecting..."); NULL or
 * "" restores the normal title. On the CGX backend this updates the window
 * title; backends without a status surface ignore it. */
void display_set_status(amiga_display *d, const char *text);

void display_close(amiga_display *d);

#endif /* AMIGA_DISPLAY_H */
