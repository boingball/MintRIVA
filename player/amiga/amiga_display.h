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

/* Allow interlaced AGA screens (up to ~640x512), so tall clips fit at full
 * resolution instead of being downscaled. Off by default (interlace flickers). */
void display_set_lace(int on);

/* Use the CD32 Akiko chip's hardware chunky->planar instead of the CPU C2P.
 * CD32 only; no effect (and unsafe) elsewhere, so gate it on --cd32. */
void display_set_akiko(int on);

/* Accumulated AGA encode / blit time in ms (0 if the AGA backend wasn't used). */
void display_aga_timing(unsigned long *enc_ms, unsigned long *blit_ms);

/* Open a display able to show w*h frames: tries RTG (cybergraphics) first, then
 * falls back to AGA. Returns NULL only if neither works. */
amiga_display *display_open(int w, int h, const char *title);

/* Name of the backend that actually opened ("RTG (CGX)" / "AGA"). */
const char *display_backend_name(amiga_display *d);

/* Blit one RGB24 frame (r,g,b bytes, `stride` bytes per row, top-down). Only
 * source rows [dy0,dy1) are redrawn; pass 0..h to draw the whole frame. */
void display_show_rgb(amiga_display *d, const unsigned char *rgb,
                      int w, int h, int stride, int dy0, int dy1);

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

void display_close(amiga_display *d);

#endif /* AMIGA_DISPLAY_H */
