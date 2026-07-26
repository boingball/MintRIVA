/* MintRIVA ReAction controller.
 *
 * This intentionally remains a separate process: native AGA/HAM playback can
 * own its custom screen, while the small controller stays on Workbench.  On
 * CGX the player is a resizable Workbench window alongside this controller.
 */
#include <exec/types.h>
#include <exec/tasks.h>
#include <dos/dos.h>
#include <intuition/intuition.h>
#include <reaction/reaction.h>
#include <classes/window.h>
#include <gadgets/button.h>
#include <gadgets/checkbox.h>
#include <gadgets/chooser.h>
#include <gadgets/getfile.h>
#include <gadgets/layout.h>
#include <gadgets/string.h>
#include <images/label.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/reaction.h>
#include <stdio.h>
#include <string.h>

enum { G_FILE = 1, G_PLAY, G_PAUSE, G_STOP, G_FF, G_MODE, G_LACE, G_2X };

static void signal_player(ULONG mask)
{
    struct Task *task;
    /* FindTask and Signal are protected together so a just-finished player
     * cannot disappear between the lookup and signal delivery. */
    Forbid();
    task = FindTask((STRPTR)"MintRIVA player");
    if (task) Signal(task, mask);
    Permit();
}

static void quote_arg(char *dst, size_t cap, const char *s)
{
    size_t n = 0;
    if (cap) dst[n++] = '"';
    while (*s && n + 3 < cap) {
        if (*s == '"' || *s == '*') dst[n++] = '*';
        dst[n++] = *s++;
    }
    if (n + 1 < cap) dst[n++] = '"';
    dst[n] = 0;
}

static void start_player(Object *win, Object *file, Object *mode,
                         Object *lace, Object *twox)
{
    char path[512], quoted[1040], args[1200];
    ULONG selected = 0, checked_lace = 0, checked_2x = 0;
    {
        STRPTR p = NULL;
        GetAttr(GETFILE_FullFile, file, (ULONG *)&p);
        if (!p || !*p) return;
        strncpy(path, p, sizeof path - 1); path[sizeof path - 1] = 0;
    }
    GetAttr(CHOOSER_Selected, mode, &selected);
    GetAttr(CHECKBOX_Checked, lace, &checked_lace);
    GetAttr(CHECKBOX_Checked, twox, &checked_2x);
    quote_arg(quoted, sizeof quoted, path);
    snprintf(args, sizeof args, "%s%s%s%s", quoted,
             selected == 0 ? " --aga" : selected == 1 ? " --aga --ham6" :
             selected == 2 ? " --aga --ham" : "",
             checked_lace && selected < 3 ? " --lace" : "",
             checked_2x && selected < 3 ? " --2x" : "");
    signal_player(SIGBREAKF_CTRL_C);
    CreateNewProcTags(NP_CommandName, (ULONG)"mrplay",
                      NP_Arguments, (ULONG)args,
                      NP_Cli, TRUE, NP_Name, (ULONG)"MintRIVA player",
                      TAG_END);
    SetWindowTitles((struct Window *)NULL, (CONST_STRPTR)-1, (CONST_STRPTR)-1);
    (void)win;
}

static void update_file_info(Object *file, Object *info)
{
    static char text[640];
    STRPTR path = NULL;
    BPTR lock;
    struct FileInfoBlock fib;
    const char *ext;
    GetAttr(GETFILE_FullFile, file, (ULONG *)&path);
    if (!path || !*path) return;
    ext = strrchr(path, '.');
    lock = Lock(path, ACCESS_READ);
    if (lock && Examine(lock, &fib))
        snprintf(text, sizeof text, "%s | type: %s | %ld bytes", path,
                 ext && ext[1] ? ext + 1 : "unknown", (long)fib.fib_Size);
    else
        snprintf(text, sizeof text, "%s | type: %s", path,
                 ext && ext[1] ? ext + 1 : "unknown");
    if (lock) UnLock(lock);
    SetAttrs(info, STRINGA_TextVal, (ULONG)text, TAG_END);
}

int main(void)
{
    Object *wo, *file, *mode, *lace, *twox, *info;
    struct Window *w;
    struct List modes;
    ULONG sigmask, result;
    UWORD code;

    NewList(&modes);
    AddTail(&modes, AllocChooserNode(CNA_Text, (ULONG)"AGA", TAG_END));
    AddTail(&modes, AllocChooserNode(CNA_Text, (ULONG)"HAM6", TAG_END));
    AddTail(&modes, AllocChooserNode(CNA_Text, (ULONG)"HAM8", TAG_END));
    AddTail(&modes, AllocChooserNode(CNA_Text, (ULONG)"CGX", TAG_END));

    file = GetFileObject, GA_ID, G_FILE, GA_RelVerify, TRUE,
        GETFILE_TitleText, (ULONG)"Choose a video", GETFILE_ReadOnly, TRUE,
        GETFILE_DrawersOnly, FALSE, End;
    mode = ChooserObject, GA_ID, G_MODE, GA_RelVerify, TRUE,
        CHOOSER_Labels, (ULONG)&modes, CHOOSER_Selected, 3, End;
    lace = CheckBoxObject, GA_ID, G_LACE, GA_Text, (ULONG)"Laced", End;
    twox = CheckBoxObject, GA_ID, G_2X, GA_Text, (ULONG)"2x", End;
    info = StringObject, GA_ReadOnly, TRUE,
        STRINGA_TextVal, (ULONG)"No file selected", End;

    wo = WindowObject,
        WA_Title, (ULONG)"MintRIVA Control",
        WA_Activate, TRUE, WA_DepthGadget, TRUE, WA_CloseGadget, TRUE,
        WINDOW_Position, WPOS_CENTERSCREEN,
        WINDOW_Layout, VLayoutObject,
            LAYOUT_AddChild, file,
            CHILD_Label, LabelObject, LABEL_Text, (ULONG)"File", End,
            LAYOUT_AddChild, HLayoutObject,
                LAYOUT_AddChild, mode,
                CHILD_Label, LabelObject, LABEL_Text, (ULONG)"Display", End,
                LAYOUT_AddChild, lace,
                LAYOUT_AddChild, twox,
            End,
            LAYOUT_AddChild, HLayoutObject,
                LAYOUT_AddChild, ButtonObject, GA_ID, G_PLAY,
                    GA_Text, (ULONG)"Play", GA_RelVerify, TRUE, End,
                LAYOUT_AddChild, ButtonObject, GA_ID, G_PAUSE,
                    GA_Text, (ULONG)"Pause", GA_RelVerify, TRUE, End,
                LAYOUT_AddChild, ButtonObject, GA_ID, G_STOP,
                    GA_Text, (ULONG)"Stop", GA_RelVerify, TRUE, End,
                LAYOUT_AddChild, ButtonObject, GA_ID, G_FF,
                    GA_Text, (ULONG)"Fast forward", GA_RelVerify, TRUE, End,
            End,
            LAYOUT_AddChild, info,
        End,
    End;
    if (!wo || !(w = (struct Window *)RA_OpenWindow(wo))) {
        if (wo) DisposeObject(wo); FreeChooserNodes(&modes); return RETURN_FAIL;
    }
    GetAttr(WINDOW_SigMask, wo, &sigmask);
    for (;;) {
        Wait(sigmask | SIGBREAKF_CTRL_C);
        while ((result = RA_HandleInput(wo, &code)) != WMHI_LASTMSG) {
            switch (result & WMHI_CLASSMASK) {
            case WMHI_CLOSEWINDOW: goto done;
            case WMHI_GADGETUP:
                switch (result & WMHI_GADGETMASK) {
                case G_FILE: update_file_info(file, info); break;
                case G_PLAY: start_player(wo, file, mode, lace, twox); break;
                case G_PAUSE: signal_player(SIGBREAKF_CTRL_D); break;
                case G_STOP: signal_player(SIGBREAKF_CTRL_C); break;
                case G_FF: signal_player(SIGBREAKF_CTRL_E); break;
                }
                break;
            }
        }
    }
done:
    signal_player(SIGBREAKF_CTRL_C);
    DisposeObject(wo);
    FreeChooserNodes(&modes);
    return RETURN_OK;
}
