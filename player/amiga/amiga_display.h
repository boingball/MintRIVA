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

/* Open a display able to show w*h frames. Returns NULL on failure. */
amiga_display *display_open(int w, int h, const char *title);

/* Blit one RGB24 frame (r,g,b bytes, `stride` bytes per row, top-down). */
void display_show_rgb(amiga_display *d, const unsigned char *rgb,
                      int w, int h, int stride);

/* Non-blocking: returns 1 if the user asked to quit (close gadget or ESC). */
int  display_poll_quit(amiga_display *d);

void display_close(amiga_display *d);

#endif /* AMIGA_DISPLAY_H */
