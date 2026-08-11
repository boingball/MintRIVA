/* MintRIVA ReAction controller.
 *
 * The GUI stays on Workbench and launches the separate mrplay executable.
 * ReAction classes are opened explicitly, following MintAMP's working setup.
 */
#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/tasks.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/displayinfo.h>
#include <graphics/gfxbase.h>
#include <cybergraphx/cybergraphics.h>
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
#include <proto/graphics.h>
#include <proto/utility.h>
#include <proto/cybergraphics.h>
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
#include "../core/mr_play_options.h"

#ifndef MRGUI_CLASS_VERSION
#define MRGUI_CLASS_VERSION 44
#endif

#define MRPLAY_STACK_SIZE 320000UL

#if defined(__GNUC__)
static const char mrgui_stack_cookie[] __attribute__((used)) =
    "$STACK:131072";
#endif

/* Runtime library bases.  CyberGfxBase is optional: a planar Workbench must
 * still be able to run the AGA/HAM controller. */
struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *UtilityBase;
struct Library *AslBase;
struct Library *CyberGfxBase;
struct Library *WindowBase;
struct Library *LayoutBase;
struct Library *ButtonBase;
struct Library *CheckBoxBase;
struct Library *ChooserBase;
struct Library *GetFileBase;
struct Library *StringBase;
struct Library *LabelBase;

enum {
    G_FILE = 1,
    G_PLAY,
    G_PAUSE,
    G_STOP,
    G_FF,
    G_MODE,
    G_C2P,
    G_H264,
    G_LACE,
    G_2X,
    G_IPTV,
    G_YOUTUBE
};

/* Chooser rows are chipset-dependent, so never infer a display mode from a
 * hard-coded row number. This map is populated alongside the labels. */
static mr_display_mode mode_values[4];
static unsigned mode_count;
static int add_chooser_node(struct List *list, const char *text);

static int chipset_has_aga(void)
{
    return GfxBase && (GfxBase->ChipRevBits0 & GFXF_AA_LISA) != 0;
}

static int chipset_has_ecs_denise(void)
{
    return GfxBase && (GfxBase->ChipRevBits0 & GFXF_HR_DENISE) != 0;
}

static int add_mode_node(struct List *list, const char *text,
                         mr_display_mode value)
{
    if (mode_count >= sizeof mode_values / sizeof mode_values[0] ||
        !add_chooser_node(list, text))
        return 0;
    mode_values[mode_count++] = value;
    return 1;
}

static int open_reaction_classes(void)
{
    IntuitionBase = (struct IntuitionBase *)OpenLibrary(
        (CONST_STRPTR)"intuition.library", 39);
    GfxBase = (struct GfxBase *)OpenLibrary(
        (CONST_STRPTR)"graphics.library", 39);
    UtilityBase = OpenLibrary((CONST_STRPTR)"utility.library", 39);
    AslBase = OpenLibrary((CONST_STRPTR)"asl.library", 39);

    /* Optional.  Its presence alone does not mean the current Workbench is
     * RTG; default_screen_is_rtg() checks the actual public-screen mode. */
    CyberGfxBase = OpenLibrary((CONST_STRPTR)"cybergraphics.library", 40);

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

    return IntuitionBase && GfxBase && UtilityBase && AslBase &&
           WindowBase && LayoutBase && ButtonBase && CheckBoxBase &&
           ChooserBase && GetFileBase && StringBase && LabelBase;
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
    if (CyberGfxBase) {
        CloseLibrary(CyberGfxBase);
        CyberGfxBase = NULL;
    }
    if (AslBase) {
        CloseLibrary(AslBase);
        AslBase = NULL;
    }
    if (UtilityBase) {
        CloseLibrary(UtilityBase);
        UtilityBase = NULL;
    }
    if (GfxBase) {
        CloseLibrary((struct Library *)GfxBase);
        GfxBase = NULL;
    }
    if (IntuitionBase) {
        CloseLibrary((struct Library *)IntuitionBase);
        IntuitionBase = NULL;
    }
}

static int default_screen_is_rtg(void)
{
    struct Screen *screen;
    ULONG mode_id;
    int is_rtg;

    if (!CyberGfxBase || !GfxBase || !IntuitionBase)
        return 0;

    screen = LockPubScreen(NULL);
    if (!screen)
        return 0;

    mode_id = GetVPModeID(&screen->ViewPort);
    is_rtg = mode_id != (ULONG)INVALID_ID && IsCyberModeID(mode_id);
    UnlockPubScreen(NULL, screen);
    return is_rtg;
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

static struct Task *find_player(void)
{
    struct Task *task;

    Forbid();
    task = FindTask((STRPTR)"MintRIVA player");
    Permit();
    return task;
}

static void signal_player(ULONG mask)
{
    struct Task *task;

    Forbid();
    task = FindTask((STRPTR)"MintRIVA player");
    if (task)
        Signal(task, mask);
    Permit();
}

static void stop_player_and_wait(void)
{
    int guard;

    signal_player(SIGBREAKF_CTRL_F);
    for (guard = 0; guard < 250 && find_player(); guard++)
        Delay(1);
}

static void set_info(Object *info, struct Window *window, const char *text);

static void read_play_options(Object *mode, Object *c2p, Object *h264,
                              Object *lace,
                              Object *twox, mr_play_options *options)
{
    ULONG selected = 0, selected_c2p = 0, selected_h264 = 0;
    ULONG checked_lace = 0, checked_2x = 0;
    mr_play_options_default(options);
    GetAttr(CHOOSER_Selected, mode, &selected);
    GetAttr(CHOOSER_Selected, c2p, &selected_c2p);
    GetAttr(CHOOSER_Selected, h264, &selected_h264);
    GetAttr(CHECKBOX_Checked, lace, &checked_lace);
    GetAttr(CHECKBOX_Checked, twox, &checked_2x);
    options->display = selected < mode_count
                     ? mode_values[selected] : MR_DISPLAY_AGA;
    options->c2p = selected_c2p == 1 ? MR_C2P_AKIKO :
                   selected_c2p == 2 ? MR_C2P_KALMS : MR_C2P_STANDARD;
    options->laced = checked_lace != 0;
    options->scale_2x = checked_2x != 0;
    options->h264_performance = selected_h264 <= MR_H264_PERF_FAST
                              ? (mr_h264_performance)selected_h264
                              : MR_H264_PERF_AUTO;
}

static void open_iptv_browser(Object *mode, Object *c2p, Object *h264,
                              Object *lace,
                              Object *twox, Object *info,
                              struct Window *window)
{
    BPTR seglist;
    struct Process *process;
    char arguments[512];
    mr_play_options options;

    read_play_options(mode, c2p, h264, lace, twox, &options);
    if (!mr_build_iptv_arguments(arguments, sizeof(arguments), &options)) {
        set_info(info, window, "Could not build IPTV playback options.");
        return;
    }

    /* Keep the directory browser in its own process so downloads and list
     * filtering can never stall the controller's transport event loop.  Load
     * the executable directly rather than asking the shell to find it: files
     * copied to an Amiga volume do not always retain the Execute protection
     * bit, which made a valid iptvgui appear as an "Unknown command". */
    seglist = LoadSeg((CONST_STRPTR)"PROGDIR:iptvgui");
    if (!seglist)
        seglist = LoadSeg((CONST_STRPTR)"iptvgui");
    if (!seglist) {
        set_info(info, window,
                 "Could not load iptvgui (keep it beside mrgui).");
        return;
    }

    process = CreateNewProcTags(
        NP_Seglist, seglist,
        NP_FreeSeglist, TRUE,
        NP_Arguments, (ULONG)arguments,
        NP_StackSize, MRPLAY_STACK_SIZE,
        NP_Cli, TRUE,
        NP_CommandName, (ULONG)"iptvgui",
        NP_Name, (ULONG)"MintRIVA IPTV",
        TAG_END);
    if (!process) {
        UnLoadSeg(seglist);
        set_info(info, window, "Could not create the iptvgui process.");
    }
}

static void open_youtube_browser(Object *mode, Object *c2p, Object *h264,
                                 Object *lace,
                                 Object *twox, Object *info,
                                 struct Window *window)
{
    BPTR seglist;
    struct Process *process;
    char arguments[512];
    mr_play_options options;

    read_play_options(mode, c2p, h264, lace, twox, &options);
    if (!mr_build_iptv_arguments(arguments, sizeof(arguments), &options)) {
        set_info(info, window, "Could not build YouTube playback options.");
        return;
    }
    seglist = LoadSeg((CONST_STRPTR)"PROGDIR:ytgui");
    if (!seglist)
        seglist = LoadSeg((CONST_STRPTR)"ytgui");
    if (!seglist) {
        set_info(info, window,
                 "Could not load ytgui (keep it beside mrgui).");
        return;
    }
    process = CreateNewProcTags(
        NP_Seglist, seglist,
        NP_FreeSeglist, TRUE,
        NP_Arguments, (ULONG)arguments,
        NP_StackSize, MRPLAY_STACK_SIZE,
        NP_Cli, TRUE,
        NP_CommandName, (ULONG)"ytgui",
        NP_Name, (ULONG)"MintRIVA YouTube",
        TAG_END);
    if (!process) {
        UnLoadSeg(seglist);
        set_info(info, window, "Could not create the ytgui process.");
    }
}

static void set_info(Object *info, struct Window *window, const char *text)
{
    if (!info || !window)
        return;

    SetGadgetAttrs((struct Gadget *)info, window, NULL,
                   STRINGA_TextVal, (ULONG)(text ? text : ""),
                   STRINGA_BufferPos, 0,
                   STRINGA_DispPos, 0,
                   TAG_DONE);
}

static void update_file_info(Object *file, Object *info,
                             struct Window *window)
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
    if (lock && Examine(lock, &fib)) {
        snprintf(text, sizeof(text), "%s | type: %s | %ld bytes", path,
                 ext && ext[1] ? ext + 1 : "unknown",
                 (long)fib.fib_Size);
    } else {
        snprintf(text, sizeof(text), "%s | type: %s", path,
                 ext && ext[1] ? ext + 1 : "unknown");
    }
    if (lock)
        UnLock(lock);

    set_info(info, window, text);
}

static void update_mode_controls(Object *mode, Object *c2p, Object *lace,
                                 Object *twox, struct Window *window)
{
    ULONG selected;
    ULONG disable_chipset_options;

    selected = 0;
    GetAttr(CHOOSER_Selected, mode, &selected);
    disable_chipset_options = selected < mode_count &&
                              mode_values[selected] == MR_DISPLAY_CGX
                            ? TRUE : FALSE;

    SetGadgetAttrs((struct Gadget *)c2p, window, NULL,
                   GA_Disabled, disable_chipset_options,
                   TAG_DONE);
    SetGadgetAttrs((struct Gadget *)lace, window, NULL,
                   GA_Disabled, disable_chipset_options,
                   TAG_DONE);
    SetGadgetAttrs((struct Gadget *)twox, window, NULL,
                   GA_Disabled, disable_chipset_options,
                   TAG_DONE);
}

static void start_player(Object *file, Object *mode, Object *c2p,
                         Object *h264,
                         Object *lace, Object *twox, Object *info,
                         struct Window *window)
{
    char path[512];
    char args[1600];
    mr_play_options options;
    STRPTR full_file;
    BPTR seglist;
    struct Process *process;

    full_file = NULL;

    GetAttr(GETFILE_FullFile, file, (ULONG *)&full_file);
    if (!full_file || !*full_file) {
        set_info(info, window, "Choose a video first.");
        return;
    }

    if (find_player()) {
        set_info(info, window,
                 "A MintRIVA player is already running; stop it first.");
        return;
    }

    strncpy(path, (const char *)full_file, sizeof(path) - 1);
    path[sizeof(path) - 1] = 0;

    read_play_options(mode, c2p, h264, lace, twox, &options);
    if (!mr_build_player_arguments(args, sizeof(args), &options, path,
                                   NULL, NULL)) {
        set_info(info, window, "Could not build player arguments.");
        return;
    }

    /* NP_CommandName only labels a CLI; it does not load an executable.
     * Load mrplay explicitly and pass its seglist to CreateNewProcTags(). */
    seglist = LoadSeg((CONST_STRPTR)"PROGDIR:mrplay");
    if (!seglist)
        seglist = LoadSeg((CONST_STRPTR)"mrplay");
    if (!seglist) {
        set_info(info, window,
                 "Could not load mrplay (keep it beside mrgui or in PATH).");
        return;
    }

    process = CreateNewProcTags(
        NP_Seglist, seglist,
        NP_FreeSeglist, TRUE,
        NP_Arguments, (ULONG)args,
        NP_StackSize, MRPLAY_STACK_SIZE,
        NP_Cli, TRUE,
        NP_CommandName, (ULONG)"mrplay",
        NP_Name, (ULONG)"MintRIVA player",
        TAG_END);

    if (!process) {
        UnLoadSeg(seglist);
        set_info(info, window, "Could not create the mrplay process.");
    }
}

int main(void)
{
    Object *window_object;
    Object *file;
    Object *mode;
    Object *c2p;
    Object *h264;
    Object *lace;
    Object *twox;
    Object *info;
    Object *layout;
    Object *controls;
    Object *buttons;
    Object *file_label;
    Object *display_label;
    Object *c2p_label;
    Object *h264_label;
    Object *play_button;
    Object *pause_button;
    Object *stop_button;
    Object *ff_button;
    Object *iptv_button;
    Object *youtube_button;
    struct Window *window;
    struct List modes;
    struct List c2p_modes;
    struct List h264_modes;
    ULONG sigmask;
    ULONG result;
    ULONG signals;
    UWORD code;
    int status;
    int have_rtg;
    int default_mode;

    window_object = NULL;
    file = NULL;
    mode = NULL;
    c2p = NULL;
    h264 = NULL;
    lace = NULL;
    twox = NULL;
    info = NULL;
    layout = NULL;
    controls = NULL;
    buttons = NULL;
    file_label = NULL;
    display_label = NULL;
    c2p_label = NULL;
    h264_label = NULL;
    play_button = NULL;
    pause_button = NULL;
    stop_button = NULL;
    ff_button = NULL;
    iptv_button = NULL;
    youtube_button = NULL;
    window = NULL;
    status = RETURN_FAIL;
    have_rtg = 0;
    default_mode = 0;
    mode_count = 0;

    modes.lh_Head = (struct Node *)&modes.lh_Tail;
    modes.lh_Tail = NULL;
    modes.lh_TailPred = (struct Node *)&modes.lh_Head;
    c2p_modes.lh_Head = (struct Node *)&c2p_modes.lh_Tail;
    c2p_modes.lh_Tail = NULL;
    c2p_modes.lh_TailPred = (struct Node *)&c2p_modes.lh_Head;
    h264_modes.lh_Head = (struct Node *)&h264_modes.lh_Tail;
    h264_modes.lh_Tail = NULL;
    h264_modes.lh_TailPred = (struct Node *)&h264_modes.lh_Head;

    if (!open_reaction_classes()) {
        fprintf(stderr, "mrgui: ReAction V%ld classes are not available.\n",
                (long)MRGUI_CLASS_VERSION);
        goto cleanup;
    }

    have_rtg = default_screen_is_rtg();
    if (!add_mode_node(&modes,
                       chipset_has_aga() ? "AGA (256 colours)" :
                       chipset_has_ecs_denise() ? "ECS (32 colours)" :
                                                  "OCS (32 colours)",
                       MR_DISPLAY_AGA) ||
        !add_mode_node(&modes, "HAM6", MR_DISPLAY_HAM6) ||
        (chipset_has_aga() &&
         !add_mode_node(&modes, "HAM8", MR_DISPLAY_HAM8)) ||
        (have_rtg && !add_mode_node(&modes, "CGX/RTG", MR_DISPLAY_CGX)))
        goto cleanup;
    if (have_rtg) default_mode = (int)mode_count - 1;
    if (!add_chooser_node(&c2p_modes, "Standard") ||
        !add_chooser_node(&c2p_modes, "CD32") ||
        !add_chooser_node(&c2p_modes, "Kalms"))
        goto cleanup;
    if (!add_chooser_node(&h264_modes, "Auto") ||
        !add_chooser_node(&h264_modes, "Quality") ||
        !add_chooser_node(&h264_modes, "Balanced") ||
        !add_chooser_node(&h264_modes, "Fast"))
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
                               CHOOSER_Selected, default_mode,
                               TAG_DONE);
    c2p = (Object *)NewObject(CHOOSER_GetClass(), NULL,
                              GA_ID, G_C2P,
                              GA_RelVerify, TRUE,
                              CHOOSER_Labels, (ULONG)&c2p_modes,
                              CHOOSER_Selected, 0,
                              TAG_DONE);
    h264 = (Object *)NewObject(CHOOSER_GetClass(), NULL,
                               GA_ID, G_H264,
                               GA_RelVerify, TRUE,
                               CHOOSER_Labels, (ULONG)&h264_modes,
                               CHOOSER_Selected, MR_H264_PERF_AUTO,
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
                               STRINGA_MaxChars, 640,
                               TAG_DONE);
    file_label = (Object *)NewObject(LABEL_GetClass(), NULL,
                                     LABEL_Text, (ULONG)"File",
                                     TAG_DONE);
    display_label = (Object *)NewObject(LABEL_GetClass(), NULL,
                                        LABEL_Text, (ULONG)"Display",
                                        TAG_DONE);
    c2p_label = (Object *)NewObject(LABEL_GetClass(), NULL,
                                    LABEL_Text, (ULONG)"C2P",
                                    TAG_DONE);
    h264_label = (Object *)NewObject(LABEL_GetClass(), NULL,
                                     LABEL_Text, (ULONG)"H.264",
                                     TAG_DONE);

    if (!file || !mode || !c2p || !h264 || !lace || !twox || !info ||
        !file_label || !display_label || !c2p_label || !h264_label)
        goto cleanup;

    controls = (Object *)NewObject(LAYOUT_GetClass(), NULL,
                                    LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ,
                                    LAYOUT_AddChild, (ULONG)mode,
                                    CHILD_Label, (ULONG)display_label,
                                    LAYOUT_AddChild, (ULONG)c2p,
                                    CHILD_Label, (ULONG)c2p_label,
                                    LAYOUT_AddChild, (ULONG)h264,
                                    CHILD_Label, (ULONG)h264_label,
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
    iptv_button = (Object *)NewObject(BUTTON_GetClass(), NULL,
                                      GA_ID, G_IPTV,
                                      GA_Text, (ULONG)"IPTV...",
                                      GA_RelVerify, TRUE,
                                      TAG_DONE);
    youtube_button = (Object *)NewObject(BUTTON_GetClass(), NULL,
                                         GA_ID, G_YOUTUBE,
                                         GA_Text, (ULONG)"YouTube...",
                                         GA_RelVerify, TRUE,
                                         TAG_DONE);
    if (!play_button || !pause_button || !stop_button || !ff_button ||
        !iptv_button || !youtube_button)
        goto cleanup;

    buttons = (Object *)NewObject(LAYOUT_GetClass(), NULL,
                                   LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ,
                                   LAYOUT_EvenSize, TRUE,
                                   LAYOUT_AddChild, (ULONG)play_button,
                                   LAYOUT_AddChild, (ULONG)pause_button,
                                   LAYOUT_AddChild, (ULONG)stop_button,
                                   LAYOUT_AddChild, (ULONG)ff_button,
                                   LAYOUT_AddChild, (ULONG)iptv_button,
                                   LAYOUT_AddChild, (ULONG)youtube_button,
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

    window_object = (Object *)NewObject(WINDOW_GetClass(), NULL,
                                         WA_Title,
                                         (ULONG)"MintRIVA Control",
                                         WA_Activate, TRUE,
                                         WA_DepthGadget, TRUE,
                                         WA_DragBar, TRUE,
                                         WA_CloseGadget, TRUE,
                                         WA_SizeGadget, TRUE,
                                         WA_IDCMP,
                                         IDCMP_GADGETUP |
                                         IDCMP_CLOSEWINDOW |
                                         IDCMP_IDCMPUPDATE |
                                         IDCMP_REFRESHWINDOW,
                                         WINDOW_Position, WPOS_CENTERSCREEN,
                                         WINDOW_ParentGroup, (ULONG)layout,
                                         TAG_DONE);
    if (!window_object)
        goto cleanup;

    window = (struct Window *)RA_OpenWindow(window_object);
    if (!window)
        goto cleanup;

    update_mode_controls(mode, c2p, lace, twox, window);
    GetAttr(WINDOW_SigMask, window_object, &sigmask);

    for (;;) {
        signals = Wait(sigmask | SIGBREAKF_CTRL_C);
        if (signals & SIGBREAKF_CTRL_C)
            break;

        while ((result = RA_HandleInput(window_object, &code)) !=
               WMHI_LASTMSG) {
            switch (result & WMHI_CLASSMASK) {
            case WMHI_CLOSEWINDOW:
                goto done;

            case WMHI_GADGETUP:
                switch (result & WMHI_GADGETMASK) {
                case G_FILE:
                    if (gfRequestFile(file, window))
                        update_file_info(file, info, window);
                    break;

                case G_MODE:
                    update_mode_controls(mode, c2p, lace, twox, window);
                    break;

                case G_PLAY:
                    start_player(file, mode, c2p, h264, lace, twox, info,
                                 window);
                    break;

                case G_PAUSE:
                    signal_player(SIGBREAKF_CTRL_D);
                    break;

                case G_STOP:
                    stop_player_and_wait();
                    break;

                case G_FF:
                    signal_player(SIGBREAKF_CTRL_E);
                    break;

                case G_IPTV:
                    open_iptv_browser(mode, c2p, h264, lace, twox, info,
                                      window);
                    break;

                case G_YOUTUBE:
                    open_youtube_browser(mode, c2p, h264, lace, twox, info,
                                         window);
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
    stop_player_and_wait();

cleanup:
    /* Dispose only the highest successfully-created owner.  The window owns
     * the root layout; layouts own their child layouts and gadgets. */
    if (window_object) {
        if (window)
            RA_CloseWindow(window_object);
        DisposeObject(window_object);
    } else if (layout) {
        DisposeObject(layout);
    } else {
        if (controls) {
            DisposeObject(controls);
        } else {
            if (mode)
                DisposeObject(mode);
            if (c2p)
                DisposeObject(c2p);
            if (h264)
                DisposeObject(h264);
            if (lace)
                DisposeObject(lace);
            if (twox)
                DisposeObject(twox);
            if (display_label)
                DisposeObject(display_label);
            if (c2p_label)
                DisposeObject(c2p_label);
            if (h264_label)
                DisposeObject(h264_label);
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
            if (iptv_button)
                DisposeObject(iptv_button);
            if (youtube_button)
                DisposeObject(youtube_button);
        }

        if (file)
            DisposeObject(file);
        if (info)
            DisposeObject(info);
        if (file_label)
            DisposeObject(file_label);
    }

    free_chooser_nodes(&modes);
    free_chooser_nodes(&c2p_modes);
    free_chooser_nodes(&h264_modes);
    close_reaction_classes();
    return status;
}
