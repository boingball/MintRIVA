/*
 * MintVID - internal display backend vtable.
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
    int   (*toggle_fullscreen)(void *handle);
    /* Optional: report whether this handle can accept a pre-dithered 8-bit
     * indexed frame via show_indexed() instead of RGB24 via show(). Only
     * ever true for the AGA backend's plain indexed configuration (depth
     * 8, no HAM, no pixel-doubling, no resize - exactly what
     * core/mr_dither_rgb8() encodes for); leave NULL (unsupported) for
     * every other backend, including AGA's own HAM/scaled/resized modes -
     * mrplay.c must not call show_indexed() unless this returned non-zero
     * for the same handle. */
    int   (*supports_indexed)(void *handle);
    /* Optional: blit a pre-dithered 8-bit indexed frame - idx_stride bytes
     * per row, one byte per pixel, already in this backend's palette index
     * space (core/mr_dither_rgb8()'s fixed 6x6x6 cube for the AGA backend).
     * Only implemented (non-NULL) where supports_indexed exists. */
    void  (*show_indexed)(void *handle, const unsigned char *idx, int w,
                          int h, int idx_stride, int dy0, int dy1,
                          mr_display_service_fn service, void *service_opaque);
} display_backend;

extern const display_backend backend_cgx;
extern const display_backend backend_p96;
extern const display_backend backend_aga;

/* AGA backend configuration, set via the public display_set_* calls. */
extern int g_aga_ham;    /* 0 = indexed planar, 6 = HAM6, 8 = AGA HAM8      */
extern int g_aga_scale;  /* 1 or 2 (pixel doubling)                        */
extern int g_aga_c2p;    /* 0 = WPA8, 1 = portable, 2 = RiVA, 3 = Kalms     */
extern int g_aga_lace;   /* 1 = allow interlaced screens (taller fit)       */
extern int g_aga_akiko;  /* 1 = use CD32 Akiko hardware C2P                  */
extern int g_aga_ecs_fast; /* 1 = force the 4-plane/16-colour fast encoder   */
extern int g_aga_ecs32;    /* 1 = force the 5-plane/32-colour ECS/OCS cube    */
extern int g_display_fullscreen;

/* Library bases opened once by display.c and shared by the backends. */
struct IntuitionBase;
struct GfxBase;
struct Library;
extern struct IntuitionBase *IntuitionBase;
extern struct GfxBase       *GfxBase;
extern struct Library       *CyberGfxBase;
/* Picasso96API.library base, opened only when the P96 backend is selected
 * (display_set_force_p96()); NULL otherwise, in which case backend_p96's
 * open() must fail so display_open() falls back to CGX/AGA. */
extern struct Library       *P96Base;

/* Set non-zero to enable timing/diagnostic printf output.  Wired to --time. */
extern int g_display_want_time;

#endif /* DISPLAY_BACKEND_H */
