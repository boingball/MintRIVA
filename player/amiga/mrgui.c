/* MintRIVA ReAction controller.
 *
 * This intentionally remains a separate process: native AGA/HAM playback can
 * own its custom screen, while the small controller stays on Workbench.  On
 * CGX the player is a resizable Workbench window alongside this controller.
 *
 * The ReAction setup follows MintAMP: system/class libraries are opened
 * explicitly at runtime, BOOPSI objects are created with NewObject(), pointer
 * tag data is cast to ULONG, and no libauto link is required.
 */
#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/tasks.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <intuition/intuition.h>
#include <classes/window.h>
#include <gadgets/button.h>
#include <gadgets/checkbox.h>
#include <gadgets/chooser.h>
#include <gadgets/getfile.h>
#include <gadgets/layout.h>
#include <gadgets/string.h>
#include <images/label.h>
#include <reaction/reaction.h>
#include <reaction/reaction_macros.h>
#include <proto/asl.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/button.h>
#include <proto/checkbox.h>
#include <proto/chooser.h>
#include <proto/getfile.h>
#include <proto/label.h>
#include <proto/layout.h>
#include <proto/string.h>
#include <proto/window.h>
#include <stdio.h>
#include <string.h>

#ifndef MRGUI_CLASS_VERSION
#define MRGUI_CLASS_VERSION 44
#endif

/* Runtime library bases, following MintAMP's ReAction frontend. */
struct IntuitionBase *IntuitionBase;
struct Library *UtilityBase;
struct Library *AslBase;
struct Library *WindowBase;
struct Library *LayoutBase;
struct Library *ButtonBase;
struct Library *CheckBoxBase;
struct Library *ChooserBase;
struct Library *GetFileBase;
struct Library *StringBase;
struct Library *LabelBase;

enum { G_FILE = 1, G_PLAY, G_PAUSE, G_STOP, G_FF, G_MODE, G_LACE, G_2X };

static int open_reaction_classes(void)
{
    IntuitionBase = (struct IntuitionBase *)OpenLibrary(
        (CONST_STRPTR)"intuition.library", 39);
    UtilityBase = OpenLibrary((CONST_STRPTR)"utility.library", 39);
    AslBase = OpenLibrary((CONST_STRPTR)"asl.library", 39);
    WindowBase = OpenLibrary((CONST_STRPTR)"window.class",
                             MRGUI_CLASS_VERSION);
    LayoutBase = OpenLibrary((CONST_STRPTR)"gadgets/layout.gadget",
                             MRGUI_CLASS_VERSION);
    ButtonBase = OpenLibrary((CONST_STRPTR)"gadgets/button.gadget",
                             MRGUI_CLASS_VERSION);
    CheckBoxBase = OpenLibrary((CONST_STRPTR)"gadgets/checkbox.gadget",
                               MRGUI_CLASS_VERSION);
    ChooserBase = OpenLibrary((CONST_STRPTR)"gadgets/chooser.gadget",
                              MRGUI_CLASS_VERSION);
    GetFileBase = OpenLibrary((CONST_STRPTR)"gadgets/getfile.gadget",
                              MRGUI_CLASS_VERSION);
    StringBase = OpenLibrary((CONST_STRPTR)"gadgets/string.gadget",
                             MRGUI_CLASS_VERSION);
    LabelBase = OpenLibrary((CONST_STRPTR)"images/label.image",
                            MRGUI_CLASS_VERSION);

    return IntuitionBase && UtilityBase && AslBase && WindowBase &&
           LayoutBase && ButtonBase && CheckBoxBase && ChooserBase &&
           GetFileBase && StringBase && LabelBase;
}

static void close_reaction_classes(void)
{
    if (LabelBase) {
        CloseLibrary(LabelBase);
        LabelBase = NULL;
    }
    if (StringBase) {
        CloseLibrary(StringBase);
        StringBase = NULL;
    }
    if (GetFileBase) {
        CloseLibrary(GetFileBase);
        GetFileBase = NULL;
    }
    if (ChooserBase) {
        CloseLibrary(ChooserBase);
        ChooserBase = NULL;
    }
    if (CheckBoxBase) {
        CloseLibrary(CheckBoxBase);
        CheckBoxBase = NULL;
    }
    if (ButtonBase) {
        CloseLibrary(ButtonBase);
        ButtonBase = NULL;
    }
    if (LayoutBase) {
        CloseLibrary(LayoutBase);
        LayoutBase = NULL;
    }
    if (WindowBase) {
        CloseLibrary(WindowBase);
        WindowBase = NULL;
    }
    if (AslBase) {
        CloseLibrary(AslBase);
        AslBase = NULL;
    }
    if (UtilityBase) {
        CloseLibrary(UtilityBase);
        UtilityBase = NULL;
    }
    if (IntuitionBase) {
        CloseLibrary((struct Library *)IntuitionBase);
        IntuitionBase = NULL;
    }
}

static int add_chooser_node(struct List *list, const char *text)
{
    struct Node *node;

    node = AllocChooserNode(CNA_Text, (ULONG)text, TAG_END);
    if (!node)
        return 0;

    AddTail(list, node);
    return 1;
}

static void free_chooser_nodes(struct List *list)
{
    struct Node *node;

    while ((node = RemHead(list)) != NULL)
        FreeChooserNode(node);
}

static void signal_player(ULONG mask)
{
    struct Task *task;

    /* FindTask and Signal are protected together so a just-finished player
     * cannot disappear between the lookup and signal delivery. */
    Forbid();
    task = FindTask((STRPTR)"MintRIVA player");
    if (task)
        Signal(task, mask);
    Permit();
}

static void quote_arg(char *dst, size_t cap, const char *s)
{
    size_t n;

    n = 0;
    if (cap)
        dst[n++] = '"';
    while (*s && n + 3 < cap) {
        if (*s == '"' || *s == '*')
            dst[n++] = '*';
        dst[n++] = *s++;
    }
    if (n + 1 < cap)
        dst[n++] = '"';
    dst[n] = 0;
}

static void start_player(Object *file, Object *mode, Object *lace, Object *twox)
{
    char path[512];
    char quoted[1040];
    char args[1200];
    ULONG selected;
    ULONG checked_lace;
    ULONG checked_2x;
    STRPTR p;

    selected = 0;
    checked_lace = 0;
    checked_2x = 0;
    p = NULL;

    GetAttr(GETFILE_FullFile, file, (ULONG *)&p);
    if (!p || !*p)
        return;

    strncpy(path, (const char *)p, sizeof(path) - 1);
    path[sizeof(path) - 1] = 0;

    GetAttr(CHOOSER_Selected, mode, &selected);
    GetAttr(CHECKBOX_Checked, lace, &checked_lace);
    GetAttr(CHECKBOX_Checked, twox, &checked_2x);
    quote_arg(quoted, sizeof(quoted), path);
    snprintf(args, sizeof(args), "%s%s%s%s", quoted,
             selected == 0 ? " --aga" :
             selected == 1 ? " --aga --ham6" :
             selected == 2 ? " --aga --ham" : "",
             checked_lace && selected < 3 ? " --lace" : "",
             checked_2x && selected < 3 ? " --2x" : "");

    signal_player(SIGBREAKF_CTRL_C);
    CreateNewProcTags(NP_CommandName, (ULONG)"mrplay",
                      NP_Arguments, (ULONG)args,
                      NP_Cli, TRUE,
                      NP_Name, (ULONG)"MintRIVA player",
                      TAG_END);
}

static void update_file_info(Object *file, Object *info)
{
    static char text[640];
    STRPTR path;
    BPTR lock;
    struct FileInfoBlock fib;
    const char *ext;

    path = NULL;
    GetAttr(GETFILE_FullFile, file, (ULONG *)&path);
    if (!path || !*path)
        return;

    ext = strrchr((const char *)path, '.');
    lock = Lock(path, ACCESS_READ);
    if (lock && Examine(lock, &fib))
        snprintf(text, sizeof(text), "%s | type: %s | %ld bytes", path,
                 ext && ext[1] ? ext + 1 : "unknown", (long)fib.fib_Size);
    else
        snprintf(text, sizeof(text), "%s | type: %s", path,
                 ext && ext[1] ? ext + 1 : "unknown");
    if (lock)
        UnLock(lock);

    SetAttrs(info, STRINGA_TextVal, (ULONG)text, TAG_END);
}

int main(void)
{
    Object *wo;
    Object *file;
    Object *mode;
    Object *lace;
    Object *twox;
    Object *info;
    Object *layout;
    Object *controls;
    Object *buttons;
    Object *file_label;
    Object *display_label;
    Object *play_button;
    Object *pause_button;
    Object *stop_button;
    Object *ff_button;
    struct Window *w;
    struct List modes;
    ULONG sigmask;
    ULONG result;
    ULONG signals;
    UWORD code;
    int status;

    wo = NULL;
    file = NULL;
    mode = NULL;
    lace = NULL;
    twox = NULL;
    info = NULL;
    layout = NULL;
    controls = NULL;
    buttons = NULL;
    file_label = NULL;
    display_label = NULL;
    play_button = NULL;
    pause_button = NULL;
    stop_button = NULL;
    ff_button = NULL;
    w = NULL;
    status = RETURN_FAIL;

    modes.lh_Head = (struct Node *)&modes.lh_Tail;
    modes.lh_Tail = NULL;
    modes.lh_TailPred = (struct Node *)&modes.lh_Head;

    if (!open_reaction_classes()) {
        fprintf(stderr, "mrgui: ReAction V%ld classes are not available.\n",
                (long)MRGUI_CLASS_VERSION);
        goto cleanup;
    }

    if (!add_chooser_node(&modes, "AGA") ||
        !add_chooser_node(&modes, "HAM6") ||
        !add_chooser_node(&modes, "HAM8") ||
        !add_chooser_node(&modes, "CGX"))
        goto cleanup;

    file = (Object *)NewObject(GETFILE_GetClass(), NULL,
                               GA_ID, G_FILE,
                               GA_RelVerify, TRUE,
                               GETFILE_TitleText, (ULONG)"Choose a video",
                               GETFILE_ReadOnly, TRUE,
                               GETFILE_DrawersOnly, FALSE,
                               TAG_DONE);
    mode = (Object *)NewObject(CHOOSER_GetClass(), NULL,
                               GA_ID, G_MODE,
                               GA_RelVerify, TRUE,
                               CHOOSER_Labels, (ULONG)&modes,
                               CHOOSER_Selected, 3,
                               TAG_DONE);
    lace = (Object *)NewObject(CHECKBOX_GetClass(), NULL,
                               GA_ID, G_LACE,
                               GA_Text, (ULONG)"Laced",
                               TAG_DONE);
    twox = (Object *)NewObject(CHECKBOX_GetClass(), NULL,
                               GA_ID, G_2X,
                               GA_Text, (ULONG)"2x",
                               TAG_DONE);
    info = (Object *)NewObject(STRING_GetClass(), NULL,
                               GA_ReadOnly, TRUE,
                               STRINGA_TextVal, (ULONG)"No file selected",
                               TAG_DONE);
    file_label = (Object *)NewObject(LABEL_GetClass(), NULL,
                                     LABEL_Text, (ULONG)"File",
                                     TAG_DONE);
    display_label = (Object *)NewObject(LABEL_GetClass(), NULL,
                                        LABEL_Text, (ULONG)"Display",
                                        TAG_DONE);

    if (!file || !mode || !lace || !twox || !info ||
        !file_label || !display_label)
        goto cleanup;

    controls = (Object *)NewObject(LAYOUT_GetClass(), NULL,
                                    LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ,
                                    LAYOUT_AddChild, (ULONG)mode,
                                    CHILD_Label, (ULONG)display_label,
                                    LAYOUT_AddChild, (ULONG)lace,
                                    LAYOUT_AddChild, (ULONG)twox,
                                    TAG_DONE);
    if (!controls)
        goto cleanup;

    play_button = (Object *)NewObject(BUTTON_GetClass(), NULL,
                                      GA_ID, G_PLAY,
                                      GA_Text, (ULONG)"Play",
                                      GA_RelVerify, TRUE,
                                      TAG_DONE);
    pause_button = (Object *)NewObject(BUTTON_GetClass(), NULL,
                                       GA_ID, G_PAUSE,
                                       GA_Text, (ULONG)"Pause",
                                       GA_RelVerify, TRUE,
                                       TAG_DONE);
    stop_button = (Object *)NewObject(BUTTON_GetClass(), NULL,
                                      GA_ID, G_STOP,
                                      GA_Text, (ULONG)"Stop",
                                      GA_RelVerify, TRUE,
                                      TAG_DONE);
    ff_button = (Object *)NewObject(BUTTON_GetClass(), NULL,
                                    GA_ID, G_FF,
                                    GA_Text, (ULONG)"Fast forward",
                                    GA_RelVerify, TRUE,
                                    TAG_DONE);
    if (!play_button || !pause_button || !stop_button || !ff_button)
        goto cleanup;

    buttons = (Object *)NewObject(LAYOUT_GetClass(), NULL,
                                   LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ,
                                   LAYOUT_EvenSize, TRUE,
                                   LAYOUT_AddChild, (ULONG)play_button,
                                   LAYOUT_AddChild, (ULONG)pause_button,
                                   LAYOUT_AddChild, (ULONG)stop_button,
                                   LAYOUT_AddChild, (ULONG)ff_button,
                                   TAG_DONE);
    if (!buttons)
        goto cleanup;

    layout = (Object *)NewObject(LAYOUT_GetClass(), NULL,
                                  LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
                                  LAYOUT_SpaceOuter, TRUE,
                                  LAYOUT_SpaceInner, TRUE,
                                  LAYOUT_AddChild, (ULONG)file,
                                  CHILD_Label, (ULONG)file_label,
                                  LAYOUT_AddChild, (ULONG)controls,
                                  CHILD_WeightedHeight, 0,
                                  LAYOUT_AddChild, (ULONG)buttons,
                                  CHILD_WeightedHeight, 0,
                                  LAYOUT_AddChild, (ULONG)info,
                                  CHILD_WeightedHeight, 0,
                                  TAG_DONE);
    if (!layout)
        goto cleanup;

    wo = (Object *)NewObject(WINDOW_GetClass(), NULL,
                              WA_Title, (ULONG)"MintRIVA Control",
                              WA_Activate, TRUE,
                              WA_DepthGadget, TRUE,
                              WA_DragBar, TRUE,
                              WA_CloseGadget, TRUE,
                              WA_SizeGadget, TRUE,
                              WA_IDCMP, IDCMP_GADGETUP | IDCMP_CLOSEWINDOW |
                                        IDCMP_IDCMPUPDATE | IDCMP_REFRESHWINDOW,
                              WINDOW_Position, WPOS_CENTERSCREEN,
                              WINDOW_ParentGroup, (ULONG)layout,
                              TAG_DONE);
    if (!wo)
        goto cleanup;

    w = (struct Window *)RA_OpenWindow(wo);
    if (!w)
        goto cleanup;

    GetAttr(WINDOW_SigMask, wo, &sigmask);
    for (;;) {
        signals = Wait(sigmask | SIGBREAKF_CTRL_C);
        if (signals & SIGBREAKF_CTRL_C)
            break;

        while ((result = RA_HandleInput(wo, &code)) != WMHI_LASTMSG) {
            switch (result & WMHI_CLASSMASK) {
            case WMHI_CLOSEWINDOW:
                goto done;
            case WMHI_GADGETUP:
                switch (result & WMHI_GADGETMASK) {
                case G_FILE:
                    update_file_info(file, info);
                    break;
                case G_PLAY:
                    start_player(file, mode, lace, twox);
                    break;
                case G_PAUSE:
                    signal_player(SIGBREAKF_CTRL_D);
                    break;
                case G_STOP:
                    signal_player(SIGBREAKF_CTRL_C);
                    break;
                case G_FF:
                    signal_player(SIGBREAKF_CTRL_E);
                    break;
                default:
                    break;
                }
                break;
            default:
                break;
            }
        }
    }

done:
    status = RETURN_OK;
    signal_player(SIGBREAKF_CTRL_C);

cleanup:
    /* Dispose only the highest successfully-created owner.  A window owns its
     * parent layout; the root layout owns its child layouts and gadgets. */
    if (wo) {
        if (w)
            RA_CloseWindow(wo);
        DisposeObject(wo);
    } else if (layout) {
        DisposeObject(layout);
    } else {
        if (controls) {
            DisposeObject(controls);
        } else {
            if (mode)
                DisposeObject(mode);
            if (lace)
                DisposeObject(lace);
            if (twox)
                DisposeObject(twox);
            if (display_label)
                DisposeObject(display_label);
        }

        if (buttons) {
            DisposeObject(buttons);
        } else {
            if (play_button)
                DisposeObject(play_button);
            if (pause_button)
                DisposeObject(pause_button);
            if (stop_button)
                DisposeObject(stop_button);
            if (ff_button)
                DisposeObject(ff_button);
        }

        if (file)
            DisposeObject(file);
        if (info)
            DisposeObject(info);
        if (file_label)
            DisposeObject(file_label);
    }

    free_chooser_nodes(&modes);
    close_reaction_classes();
    return status;
}
