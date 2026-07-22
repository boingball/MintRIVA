/*
 * MintRIVA - internal display backend vtable.
 *
 * Each backend (RTG/cybergraphics, AGA) implements these four calls over its
 * own opaque handle; display.c picks one and routes the public API to it.
 */
#ifndef DISPLAY_BACKEND_H
#define DISPLAY_BACKEND_H

typedef struct {
    const char *name;
    void *(*open)(int w, int h, const char *title);
    /* dy0..dy1 are the changed source rows to (re)draw; the rest of the display
     * is left untouched (it persists from the previous frame). */
    void  (*show)(void *handle, const unsigned char *rgb, int w, int h,
                  int stride, int dy0, int dy1);
    int   (*poll)(void *handle);
    void  (*close)(void *handle);
} display_backend;

extern const display_backend backend_cgx;
extern const display_backend backend_aga;

/* AGA backend configuration, set via the public display_set_* calls. */
extern int g_aga_ham;    /* 0 = 256-colour dither, 6 = HAM6, 8 = HAM8       */
extern int g_aga_scale;  /* 1 or 2 (pixel doubling)                        */
extern int g_aga_c2p;    /* 1 = fast mr_c2p8 (default), 0 = WritePixelArray8 */
extern int g_aga_lace;   /* 1 = allow interlaced screens (taller fit)       */
extern int g_aga_akiko;  /* 1 = use CD32 Akiko hardware C2P                  */

/* Library bases opened once by display.c and shared by the backends. */
#include <exec/types.h>
struct IntuitionBase;
struct GfxBase;
struct Library;
extern struct IntuitionBase *IntuitionBase;
extern struct GfxBase       *GfxBase;
extern struct Library       *CyberGfxBase;

#endif /* DISPLAY_BACKEND_H */
