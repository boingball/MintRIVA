/*
 * MintRIVA - internal display backend vtable.
 *
 * Each backend (RTG/cybergraphics, AGA) implements these four calls over its
 * own opaque handle; display.c picks one and routes the public API to it.
 */
#ifndef DISPLAY_BACKEND_H
#define DISPLAY_BACKEND_H

#include <exec/types.h>

typedef struct {
    const char *name;
    void *(*open)(int w, int h, const char *title);
    /* dy0..dy1 are the changed source rows to (re)draw; the rest of the display
     * is left untouched (it persists from the previous frame). */
    void  (*show)(void *handle, const unsigned char *rgb, int w, int h,
                  int stride, int dy0, int dy1,
                  mr_display_service_fn service, void *service_opaque);
    int   (*timing)(void *handle, mr_display_timing *timing);
    int   (*poll)(void *handle);
    void  (*close)(void *handle);
    /* Optional: show a short status string (NULL/empty restores the normal
     * title). Backends may leave this NULL. */
    void  (*status)(void *handle, const char *text);
    /* Optional: signal bits a caller may Wait() on to wake for backend events. */
    ULONG (*wait_mask)(void *handle);
} display_backend;

extern const display_backend backend_cgx;
extern const display_backend backend_aga;

/* AGA backend configuration, set via the public display_set_* calls. */
extern int g_aga_ham;    /* 0 = 256-colour dither, 6 = HAM6, 8 = HAM8       */
extern int g_aga_scale;  /* 1 or 2 (pixel doubling)                        */
extern int g_aga_c2p;    /* 0 = WPA8, 1 = portable, 2 = RiVA, 3 = Kalms     */
extern int g_aga_lace;   /* 1 = allow interlaced screens (taller fit)       */
extern int g_aga_akiko;  /* 1 = use CD32 Akiko hardware C2P                  */

/* Library bases opened once by display.c and shared by the backends. */
struct IntuitionBase;
struct GfxBase;
struct Library;
extern struct IntuitionBase *IntuitionBase;
extern struct GfxBase       *GfxBase;
extern struct Library       *CyberGfxBase;

/* Set non-zero to enable timing/diagnostic printf output.  Wired to --time. */
extern int g_display_want_time;

#endif /* DISPLAY_BACKEND_H */
