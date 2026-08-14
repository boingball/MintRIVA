/* MintVID-GT: OS 3.1 GadTools controller for the shared mrplay engine. */
#include "../core/mr_play_options.h"
#include "mr_gui_menu.h"
#include "mr_master_options.h"

#include <cybergraphx/cybergraphics.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <graphics/displayinfo.h>
#include <graphics/gfxbase.h>
#include <graphics/text.h>
#include <intuition/intuition.h>
#include <libraries/asl.h>
#include <libraries/gadtools.h>
#include <proto/asl.h>
#include <proto/cybergraphics.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/gadtools.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <stdio.h>
#include <string.h>

#define MRPLAY_STACK_SIZE 320000UL
#define WIN_W 640
#define WIN_H 130

#if defined(__GNUC__)
static const char mrgui_gt_stack_cookie[] __attribute__((used))="$STACK:131072";
#endif

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *AslBase;
struct Library *CyberGfxBase;
extern struct Library *GadToolsBase;

enum {
    G_FILE = 1, G_BROWSE, G_MODE, G_C2P, G_H264, G_LACE, G_2X,
    G_PLAY, G_PAUSE, G_STOP, G_FAST, G_IPTV, G_YOUTUBE, G_INFO
};

typedef struct gt_app {
    struct Screen *screen;
    struct Window *window;
    APTR visual;
    struct Gadget *gadgets;
    struct Gadget *file, *mode, *c2p, *h264, *lace, *twox, *info;
    struct FileRequester *requester;
    mr_master_options_port *master;
    mr_gui_menu menu;
    mr_display_mode modes[7];
    STRPTR mode_labels[8];
    unsigned mode_count;
    char path[512];
} gt_app;

static STRPTR c2p_labels[] = {(STRPTR)"C2P: Standard", (STRPTR)"C2P: CD32",
                             (STRPTR)"C2P: Kalms", NULL};
static STRPTR h264_labels[] = {(STRPTR)"H.264: Auto", (STRPTR)"H.264: Quality",
                              (STRPTR)"H.264: Balanced", (STRPTR)"H.264: Fast", NULL};
static const struct TextAttr topaz = {(STRPTR)"topaz.font", 8, 0, 0};

static int aga(void)
{
    return GfxBase && (GfxBase->ChipRevBits0 & GFXF_AA_LISA) != 0;
}

static int ecs(void)
{
    return GfxBase && (GfxBase->ChipRevBits0 & GFXF_HR_DENISE) != 0;
}

static int screen_is_rtg(struct Screen *screen)
{
    ULONG id;
    if (!screen || !CyberGfxBase)
        return 0;
    id = GetVPModeID(&screen->ViewPort);
    return id != (ULONG)INVALID_ID && IsCyberModeID(id);
}

static void set_info(gt_app *app, const char *text)
{
    if (app->window && app->info)
        GT_SetGadgetAttrs(app->info, app->window, NULL,
                         GTTX_Text, (ULONG)(text ? text : ""), TAG_DONE);
    if (app->window && app->info)
        RefreshGList(app->info, app->window, NULL, 1);
}

static ULONG gad_value(gt_app *app, struct Gadget *gadget, ULONG tag)
{
    ULONG value = 0;
    GT_GetGadgetAttrs(gadget, app->window, NULL, tag, (ULONG)&value, TAG_DONE);
    return value;
}

static void read_options(gt_app *app, mr_play_options *options)
{
    ULONG mode = gad_value(app, app->mode, GTCY_Active);
    ULONG c2p = gad_value(app, app->c2p, GTCY_Active);
    ULONG h264 = gad_value(app, app->h264, GTCY_Active);
    mr_play_options_default(options);
    options->display = mode < app->mode_count ? app->modes[mode]
                                               : MR_DISPLAY_AGA;
    options->c2p = c2p == 1 ? MR_C2P_AKIKO :
                   c2p == 2 ? MR_C2P_KALMS : MR_C2P_STANDARD;
    options->h264_performance = h264 <= MR_H264_PERF_FAST
                              ? (mr_h264_performance)h264
                              : MR_H264_PERF_AUTO;
    options->laced = gad_value(app, app->lace, GTCB_Checked) != 0;
    options->scale_2x = gad_value(app, app->twox, GTCB_Checked) != 0;
}

static void publish_options(gt_app *app)
{
    mr_play_options options;
    read_options(app, &options);
    mr_master_options_publish(app->master, &options);
}

static void update_mode_controls(gt_app *app)
{
    ULONG selected = gad_value(app, app->mode, GTCY_Active);
    ULONG disabled = selected < app->mode_count &&
                     (app->modes[selected] == MR_DISPLAY_CGX ||
                      app->modes[selected] == MR_DISPLAY_P96);
    GT_SetGadgetAttrs(app->c2p, app->window, NULL,
                     GA_Disabled, disabled, TAG_DONE);
    GT_SetGadgetAttrs(app->lace, app->window, NULL,
                     GA_Disabled, disabled, TAG_DONE);
    GT_SetGadgetAttrs(app->twox, app->window, NULL,
                     GA_Disabled, disabled, TAG_DONE);
}

static struct Task *find_player(void)
{
    struct Task *task;
    Forbid();
    task = FindTask((STRPTR)"MintVID player");
    Permit();
    return task;
}

static void signal_player(ULONG mask)
{
    struct Task *task;
    Forbid();
    task = FindTask((STRPTR)"MintVID player");
    if (task)
        Signal(task, mask);
    Permit();
}

static void stop_player(void)
{
    unsigned guard;
    signal_player(SIGBREAKF_CTRL_F);
    for (guard = 0; guard < 250 && find_player(); guard++)
        Delay(1);
}

static int launch(const char *program, const char *task_name,
                  const char *arguments)
{
    char path[64];
    BPTR seglist;
    struct Process *process;
    snprintf(path, sizeof(path), "PROGDIR:%s", program);
    seglist = LoadSeg((CONST_STRPTR)path);
    if (!seglist)
        seglist = LoadSeg((CONST_STRPTR)program);
    if (!seglist)
        return 0;
    process = CreateNewProcTags(NP_Seglist, seglist, NP_FreeSeglist, TRUE,
        NP_Arguments, (ULONG)(arguments ? arguments : ""),
        NP_StackSize, MRPLAY_STACK_SIZE, NP_Cli, TRUE,
        NP_CommandName, (ULONG)program, NP_Name, (ULONG)task_name, TAG_END);
    if (!process)
        UnLoadSeg(seglist);
    return process != NULL;
}

static void play_file(gt_app *app)
{
    mr_play_options options;
    char args[1600];
    STRPTR entered = NULL;
    GT_GetGadgetAttrs(app->file, app->window, NULL,
                     GTST_String, (ULONG)&entered, TAG_DONE);
    if (entered) {
        strncpy(app->path, (const char *)entered, sizeof(app->path) - 1);
        app->path[sizeof(app->path) - 1] = 0;
    }
    if (!app->path[0]) {
        set_info(app, "Choose a video first.");
        return;
    }
    if (mr_path_is_audio_only(app->path)) {
        set_info(app, "Audio-only file: use MintAMP instead.");
        return;
    }
    if (find_player()) {
        set_info(app, "A MintVID player is already running; stop it first.");
        return;
    }
    read_options(app, &options);
    if (!mr_build_player_arguments(args, sizeof(args), &options, app->path,
                                   NULL, NULL) ||
        !launch("mrplay", "MintVID player", args))
        set_info(app, "Could not start mrplay (keep it beside mrgui-GT).");
    else
        set_info(app, "Starting playback...");
}

static void browse(gt_app *app)
{
    if (!app->requester)
        app->requester = AllocAslRequestTags(ASL_FileRequest,
            ASLFR_TitleText, (ULONG)"Choose a video", ASLFR_DoSaveMode, FALSE,
            ASLFR_RejectIcons, TRUE, ASLFR_DoPatterns, TRUE,
            ASLFR_InitialPattern, (ULONG)MR_VIDEO_FILE_PATTERN, TAG_DONE);
    if (!app->requester ||
        !AslRequestTags(app->requester, ASLFR_Window, (ULONG)app->window,
                        TAG_DONE))
        return;
    strncpy(app->path, (const char *)app->requester->fr_Drawer,
            sizeof(app->path) - 1);
    app->path[sizeof(app->path) - 1] = 0;
    if (!AddPart((STRPTR)app->path, app->requester->fr_File,
                 sizeof(app->path))) {
        set_info(app, "The selected path is too long.");
        return;
    }
    GT_SetGadgetAttrs(app->file, app->window, NULL,
                     GTST_String, (ULONG)app->path, TAG_DONE);
    set_info(app, app->path);
}

static void open_browser(gt_app *app, int youtube)
{
    mr_play_options options;
    char args[512];
    const char *program = youtube ? "ytgui-GT" : "iptvgui-GT";
    const char *task = youtube ? "MintVID YouTube GT" : "MintVID IPTV GT";
    read_options(app, &options);
    publish_options(app);
    if (!mr_build_iptv_arguments(args, sizeof(args), &options) ||
        !launch(program, task, args))
        set_info(app, youtube ? "Could not start ytgui-GT."
                              : "Could not start iptvgui-GT.");
}

static struct Gadget *add_gadget(gt_app *app, struct Gadget *previous,
                                 ULONG kind, UWORD id, int x, int y, int w,
                                 int h, const char *label, ULONG tag1,
                                 ULONG data1)
{
    struct NewGadget ng;
    memset(&ng, 0, sizeof(ng));
    ng.ng_LeftEdge = x;
    ng.ng_TopEdge = y;
    ng.ng_Width = w;
    ng.ng_Height = h;
    ng.ng_GadgetText = (STRPTR)label;
    ng.ng_TextAttr = (struct TextAttr *)&topaz;
    ng.ng_GadgetID = id;
    ng.ng_VisualInfo = app->visual;
    ng.ng_Flags = kind == BUTTON_KIND ? PLACETEXT_IN :
                  kind == CHECKBOX_KIND ? PLACETEXT_RIGHT : PLACETEXT_LEFT;
    if (kind == TEXT_KIND)
        return CreateGadget(kind, previous, &ng, tag1, data1,
                            GTTX_Border, TRUE, TAG_DONE);
    return CreateGadget(kind, previous, &ng, tag1, data1, TAG_DONE);
}

static int build_window(gt_app *app)
{
    struct Gadget *g;
    struct NewWindow nw;
    int default_mode = 0;

    app->screen = LockPubScreen(NULL);
    if (!app->screen)
        return 0;
    app->visual = GetVisualInfoA(app->screen, NULL);
    if (!app->visual || !CreateContext(&app->gadgets))
        return 0;

    if (aga()) {
        app->mode_labels[app->mode_count] = (STRPTR)"Display: AGA (256)";
        app->modes[app->mode_count++] = MR_DISPLAY_AGA;
        app->mode_labels[app->mode_count] = (STRPTR)"Display: ECS (32)";
        app->modes[app->mode_count++] = MR_DISPLAY_AGA_ECS32;
        app->mode_labels[app->mode_count] = (STRPTR)"Display: ECS (16)";
        app->modes[app->mode_count++] = MR_DISPLAY_AGA_ECS16;
    } else {
        app->mode_labels[app->mode_count] = ecs() ? (STRPTR)"Display: ECS" :
                                                     (STRPTR)"Display: OCS";
        app->modes[app->mode_count++] = MR_DISPLAY_AGA;
    }
    app->mode_labels[app->mode_count] = (STRPTR)"Display: HAM6";
    app->modes[app->mode_count++] = MR_DISPLAY_HAM6;
    if (aga()) {
        app->mode_labels[app->mode_count] = (STRPTR)"Display: HAM8";
        app->modes[app->mode_count++] = MR_DISPLAY_HAM8;
    }
    if (screen_is_rtg(app->screen)) {
        app->mode_labels[app->mode_count] = (STRPTR)"Display: RTG (WritePixel)";
        app->modes[app->mode_count++] = MR_DISPLAY_CGX;
        default_mode = (int)app->mode_count - 1;
        app->mode_labels[app->mode_count] = (STRPTR)"Display: RTG (P96)";
        app->modes[app->mode_count++] = MR_DISPLAY_P96;
    }
    app->mode_labels[app->mode_count] = NULL;

    g = app->gadgets;
    app->file = g = add_gadget(app, g, STRING_KIND, G_FILE, 56, 20, 493, 15,
        "", GTST_MaxChars, sizeof(app->path));
    g = add_gadget(app, g, BUTTON_KIND, G_BROWSE, 555, 20, 72, 15,
                   "Browse...", TAG_IGNORE, 0);
    app->mode = g = add_gadget(app, g, CYCLE_KIND, G_MODE, 8, 44, 160, 16,
        "", GTCY_Labels, (ULONG)app->mode_labels);
    app->c2p = g = add_gadget(app, g, CYCLE_KIND, G_C2P, 174, 44, 145, 16,
        "", GTCY_Labels, (ULONG)c2p_labels);
    app->h264 = g = add_gadget(app, g, CYCLE_KIND, G_H264, 325, 44, 170, 16,
        "", GTCY_Labels, (ULONG)h264_labels);
    app->lace = g = add_gadget(app, g, CHECKBOX_KIND, G_LACE, 501, 45, 80, 14,
        "Laced", GTCB_Checked, FALSE);
    app->twox = g = add_gadget(app, g, CHECKBOX_KIND, G_2X, 587, 45, 45, 14,
        "2x", GTCB_Checked, FALSE);
    g = add_gadget(app, g, BUTTON_KIND, G_PLAY, 8, 68, 75, 18, "Play",
                   TAG_IGNORE, 0);
    g = add_gadget(app, g, BUTTON_KIND, G_PAUSE, 87, 68, 75, 18, "Pause",
                   TAG_IGNORE, 0);
    g = add_gadget(app, g, BUTTON_KIND, G_STOP, 166, 68, 75, 18, "Stop",
                   TAG_IGNORE, 0);
    g = add_gadget(app, g, BUTTON_KIND, G_FAST, 245, 68, 100, 18, "Fast",
                   TAG_IGNORE, 0);
    g = add_gadget(app, g, BUTTON_KIND, G_IPTV, 349, 68, 132, 18, "IPTV...",
                   TAG_IGNORE, 0);
    g = add_gadget(app, g, BUTTON_KIND, G_YOUTUBE, 485, 68, 142, 18,
                   "YouTube...", TAG_IGNORE, 0);
    app->info = g = add_gadget(app, g, TEXT_KIND, G_INFO, 8, 92, 619, 16,
        "", GTTX_Text, (ULONG)"No file selected");
    if (!g)
        return 0;

    memset(&nw, 0, sizeof(nw));
    nw.LeftEdge = (app->screen->Width - WIN_W) / 2;
    nw.TopEdge = (app->screen->Height - WIN_H) / 2;
    nw.Width = WIN_W;
    nw.Height = WIN_H;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.IDCMPFlags = IDCMP_GADGETUP | IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW |
                    IDCMP_MENUPICK;
    nw.Flags = WFLG_CLOSEGADGET | WFLG_DRAGBAR | WFLG_DEPTHGADGET |
               WFLG_ACTIVATE | WFLG_SMART_REFRESH;
    nw.FirstGadget = app->gadgets;
    nw.Title = (UBYTE *)"MintVID Control-GT";
    nw.Screen = app->screen;
    nw.Type = CUSTOMSCREEN;
    app->window = OpenWindow(&nw);
    if (app->window)
        GT_SetGadgetAttrs(app->mode, app->window, NULL,
                         GTCY_Active, default_mode, TAG_DONE);
    return app->window != NULL;
}

static void cleanup(gt_app *app)
{
    mr_gui_menu_close(&app->menu, app->window);
    mr_master_options_close(&app->master);
    if (app->window)
        CloseWindow(app->window);
    if (app->gadgets)
        FreeGadgets(app->gadgets);
    if (app->requester)
        FreeAslRequest(app->requester);
    if (app->visual)
        FreeVisualInfo(app->visual);
    if (app->screen)
        UnlockPubScreen(NULL, app->screen);
}

int main(void)
{
    gt_app app;
    ULONG mask;
    int done = 0, rc = RETURN_FAIL;
    memset(&app, 0, sizeof(app));
    IntuitionBase = (struct IntuitionBase *)OpenLibrary(
        (CONST_STRPTR)"intuition.library", 37);
    GfxBase = (struct GfxBase *)OpenLibrary((CONST_STRPTR)"graphics.library", 37);
    AslBase = OpenLibrary((CONST_STRPTR)"asl.library", 37);
    GadToolsBase = OpenLibrary((CONST_STRPTR)"gadtools.library", 37);
    CyberGfxBase = OpenLibrary((CONST_STRPTR)"cybergraphics.library", 40);
    if (!IntuitionBase || !GfxBase || !AslBase || !GadToolsBase ||
        !build_window(&app))
        goto out;
    app.master = mr_master_options_open();
    update_mode_controls(&app);
    publish_options(&app);
    mr_gui_menu_open(&app.menu, app.window);
    mask = 1UL << app.window->UserPort->mp_SigBit;
    while (!done) {
        struct IntuiMessage *msg;
        ULONG signals = Wait(mask | SIGBREAKF_CTRL_C);
        if (signals & SIGBREAKF_CTRL_C)
            break;
        while ((msg = GT_GetIMsg(app.window->UserPort)) != NULL) {
            ULONG cls = msg->Class;
            UWORD code = msg->Code;
            struct Gadget *gadget = (struct Gadget *)msg->IAddress;
            UWORD id = gadget ? gadget->GadgetID : 0;
            GT_ReplyIMsg(msg);
            if (cls == IDCMP_CLOSEWINDOW)
                done = 1;
            else if (cls == IDCMP_REFRESHWINDOW) {
                GT_BeginRefresh(app.window);
                GT_EndRefresh(app.window, TRUE);
            } else if (cls == IDCMP_MENUPICK) {
                int action = mr_gui_menu_action(&app.menu, code);
                if (action == MR_GUI_MENU_ABOUT)
                    mr_gui_show_about(app.window, "GadTools edition (OS 3.1)");
                else if (action == MR_GUI_MENU_QUIT)
                    done = 1;
            } else if (cls == IDCMP_GADGETUP) {
                switch (id) {
                case G_BROWSE: browse(&app); break;
                case G_PLAY: play_file(&app); break;
                case G_PAUSE: signal_player(SIGBREAKF_CTRL_D); break;
                case G_STOP: stop_player(); break;
                case G_FAST: signal_player(SIGBREAKF_CTRL_E); break;
                case G_IPTV: open_browser(&app, 0); break;
                case G_YOUTUBE: open_browser(&app, 1); break;
                case G_MODE:
                    update_mode_controls(&app);
                    publish_options(&app);
                    break;
                case G_C2P: case G_H264: case G_LACE: case G_2X:
                    publish_options(&app); break;
                default: break;
                }
            }
        }
    }
    rc = RETURN_OK;
    stop_player();
out:
    cleanup(&app);
    if (CyberGfxBase) CloseLibrary(CyberGfxBase);
    if (GadToolsBase) { CloseLibrary(GadToolsBase); GadToolsBase = NULL; }
    if (AslBase) CloseLibrary(AslBase);
    if (GfxBase) CloseLibrary((struct Library *)GfxBase);
    if (IntuitionBase) CloseLibrary((struct Library *)IntuitionBase);
    return rc;
}
