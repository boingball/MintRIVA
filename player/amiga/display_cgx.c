/*
 * MintRIVA - cybergraphics RTG window display backend.
 *
 * Opens a titled window on the default public screen (which on SAGA/P96/CGX is
 * a truecolour RTG screen) and blits RGB24 frames into it with WritePixelArray,
 * letting cybergraphics do the RGB->screen-depth conversion. Input (close
 * gadget, ESC) is read from the window's IDCMP port.
 *
 * This is deliberately the simple, portable-across-RTG path. A fullscreen
 * custom-screen backend and direct RGB565 output are follow-ups.
 */
#include "amiga_display.h"

#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <cybergraphx/cybergraphics.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>

/* Library bases (opened here; SysBase/DOSBase are provided by the C startup). */
struct Library     *CyberGfxBase = NULL;
struct IntuitionBase *IntuitionBase = NULL;
struct GfxBase       *GfxBase = NULL;

struct amiga_display {
    struct Window *win;
    int            bl, bt;   /* window border offsets (blit origin)         */
    int            w, h;
    int            quit;
};

#define ESC_RAWKEY 0x45

amiga_display *display_open(int w, int h, const char *title)
{
    amiga_display *d;

    IntuitionBase = (struct IntuitionBase *)
                    OpenLibrary((CONST_STRPTR)"intuition.library", 39);
    GfxBase       = (struct GfxBase *)
                    OpenLibrary((CONST_STRPTR)"graphics.library", 39);
    CyberGfxBase  = OpenLibrary((CONST_STRPTR)"cybergraphics.library", 40);
    if (!IntuitionBase || !GfxBase || !CyberGfxBase) { display_close(NULL); return NULL; }

    d = (amiga_display *)AllocVec(sizeof *d, MEMF_CLEAR);
    if (!d) { display_close(NULL); return NULL; }
    d->w = w; d->h = h;

    d->win = OpenWindowTags(NULL,
        WA_Title,        (ULONG)(title ? title : "MintRIVA"),
        WA_InnerWidth,   (ULONG)w,
        WA_InnerHeight,  (ULONG)h,
        WA_Flags,        WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_CLOSEGADGET |
                         WFLG_ACTIVATE | WFLG_NOCAREREFRESH,
        WA_IDCMP,        IDCMP_CLOSEWINDOW | IDCMP_RAWKEY,
        TAG_END);
    if (!d->win) { display_close(d); return NULL; }

    d->bl = d->win->BorderLeft;
    d->bt = d->win->BorderTop;
    return d;
}

void display_show_rgb(amiga_display *d, const unsigned char *rgb,
                      int w, int h, int stride)
{
    if (!d || !d->win) return;
    WritePixelArray((APTR)rgb, 0, 0, (UWORD)stride,
                    d->win->RPort, (UWORD)d->bl, (UWORD)d->bt,
                    (UWORD)w, (UWORD)h, RECTFMT_RGB);
}

int display_poll_quit(amiga_display *d)
{
    struct IntuiMessage *msg;
    if (!d || !d->win) return 1;
    while ((msg = (struct IntuiMessage *)GetMsg(d->win->UserPort))) {
        ULONG  cls  = msg->Class;
        UWORD  code = msg->Code;
        ReplyMsg((struct Message *)msg);
        if (cls == IDCMP_CLOSEWINDOW) d->quit = 1;
        else if (cls == IDCMP_RAWKEY && code == ESC_RAWKEY) d->quit = 1;
    }
    return d->quit;
}

void display_close(amiga_display *d)
{
    if (d) {
        if (d->win) CloseWindow(d->win);
        FreeVec(d);
    }
    if (CyberGfxBase)  { CloseLibrary(CyberGfxBase);              CyberGfxBase = NULL; }
    if (GfxBase)       { CloseLibrary((struct Library *)GfxBase); GfxBase = NULL; }
    if (IntuitionBase) { CloseLibrary((struct Library *)IntuitionBase); IntuitionBase = NULL; }
}
