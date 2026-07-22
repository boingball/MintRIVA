/*
 * MintRIVA - display front end.
 *
 * Opens the shared library bases, then tries the RTG (cybergraphics) backend
 * and falls back to AGA - so the same mrplay runs on a PiStorm/RTG box and on a
 * plain AGA machine with no RTG. display_set_force_aga() skips the RTG attempt
 * (useful for testing AGA on an RTG machine).
 */
#include "amiga_display.h"
#include "display_backend.h"

#include <proto/exec.h>
#include <stdlib.h>

/* Single definitions of the shared library bases (the proto inlines and the
 * backends reference these globals). */
struct IntuitionBase *IntuitionBase = NULL;
struct GfxBase       *GfxBase       = NULL;
struct Library       *CyberGfxBase  = NULL;

static int g_force_aga = 0;
int g_aga_ham   = 0;   /* shared with the AGA backend */
int g_aga_scale = 1;
int g_aga_c2p   = 0;   /* WritePixelArray8 by default (measured faster);
                        * --c2p opts into the built-in transpose C2P */
int g_aga_lace  = 0;
int g_aga_akiko = 0;

void display_set_force_aga(int on) { g_force_aga = on; }
void display_set_ham(int bits) { g_aga_ham = bits; if (bits) g_force_aga = 1; }
void display_set_scale(int n)  { g_aga_scale = (n == 2) ? 2 : 1; }
void display_set_c2p(int on)   { g_aga_c2p = on ? 1 : 0; }
void display_set_lace(int on)  { g_aga_lace = on ? 1 : 0; }
void display_set_akiko(int on) { g_aga_akiko = on ? 1 : 0; }

struct amiga_display {
    const display_backend *be;
    void                  *h;
};

static void close_libs(void)
{
    if (CyberGfxBase)  { CloseLibrary(CyberGfxBase);                    CyberGfxBase  = NULL; }
    if (GfxBase)       { CloseLibrary((struct Library *)GfxBase);       GfxBase       = NULL; }
    if (IntuitionBase) { CloseLibrary((struct Library *)IntuitionBase); IntuitionBase = NULL; }
}

amiga_display *display_open(int w, int h, const char *title)
{
    const display_backend *order[2];
    int n = 0, i;

    IntuitionBase = (struct IntuitionBase *)
                    OpenLibrary((CONST_STRPTR)"intuition.library", 39);
    GfxBase       = (struct GfxBase *)
                    OpenLibrary((CONST_STRPTR)"graphics.library", 39);
    if (!IntuitionBase || !GfxBase) { close_libs(); return NULL; }

    /* RTG is optional; its absence is exactly when we want the AGA fallback. */
    CyberGfxBase = OpenLibrary((CONST_STRPTR)"cybergraphics.library", 40);

    if (!g_force_aga && CyberGfxBase) order[n++] = &backend_cgx;
    order[n++] = &backend_aga;

    for (i = 0; i < n; i++) {
        void *hh = order[i]->open(w, h, title);
        if (hh) {
            amiga_display *d = (amiga_display *)malloc(sizeof *d);
            if (!d) { order[i]->close(hh); close_libs(); return NULL; }
            d->be = order[i];
            d->h  = hh;
            return d;
        }
    }
    close_libs();
    return NULL;
}

void display_show_rgb(amiga_display *d, const unsigned char *rgb,
                      int w, int h, int stride, int dy0, int dy1)
{
    if (d) d->be->show(d->h, rgb, w, h, stride, dy0, dy1);
}

int display_poll_event(amiga_display *d)
{
    return d ? d->be->poll(d->h) : MR_EV_QUIT;
}

void display_close(amiga_display *d)
{
    if (!d) return;
    d->be->close(d->h);
    free(d);
    close_libs();
}

const char *display_backend_name(amiga_display *d)
{
    return d ? d->be->name : "none";
}
