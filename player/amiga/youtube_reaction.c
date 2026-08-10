/* Standalone MintRIVA YouTube search window.
 *
 * Search uses YouTube's public results page (no account or API key), while
 * playback stays in the normal mrplay process and its native live resolver.
 */
#include "../core/mr_http.h"
#include "../core/mr_alloc.h"
#include "../core/mr_play_options.h"
#include "../core/mr_source.h"
#include "../iptv/mr_iptv.h"
#include "../youtube/mr_youtube_search.h"

#include <classes/window.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <exec/libraries.h>
#include <exec/ports.h>
#include <exec/types.h>
#include <gadgets/button.h>
#include <gadgets/checkbox.h>
#include <gadgets/layout.h>
#include <gadgets/listbrowser.h>
#include <gadgets/string.h>
#include <images/label.h>
#include <intuition/intuition.h>
#include <proto/button.h>
#include <proto/checkbox.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/label.h>
#include <proto/layout.h>
#include <proto/listbrowser.h>
#include <proto/string.h>
#include <proto/utility.h>
#include <proto/window.h>
#include <reaction/reaction.h>
#include <reaction/reaction_macros.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define YT_CLASS_VERSION 44
#define YT_SEARCH_PAGE_MAX (6UL * 1024UL * 1024UL)
#define MRPLAY_STACK_SIZE 320000UL

struct IntuitionBase *IntuitionBase;
struct Library *UtilityBase, *WindowBase, *LayoutBase, *ButtonBase;
struct Library *CheckBoxBase, *ListBrowserBase, *StringBase, *LabelBase;

enum {
    G_QUERY = 1,
    G_SEARCH,
    G_LIVE_ONLY,
    G_RESULTS,
    G_PLAY,
    G_QUALITY,
    G_STOP,
    G_CLOSE
};

static const char *quality_labels[] = {
    "Quality: Low", "Quality: 360p", "Quality: 480p",
    "Quality: 720p", "Quality: 1080p", "Quality: Best"
};

static unsigned quality_from_options(const mr_play_options *options)
{
    if (options->hls_low)
        return 0;
    if (options->hls_max_height) {
        if (options->hls_max_height <= 360) return 1;
        if (options->hls_max_height <= 480) return 2;
        if (options->hls_max_height <= 720) return 3;
        if (options->hls_max_height <= 1080) return 4;
    }
    if (!options->hls_max_height && options->hls_max_width) {
        if (options->hls_max_width <= 640) return 1;
        if (options->hls_max_width <= 854) return 2;
        if (options->hls_max_width <= 1280) return 3;
        if (options->hls_max_width <= 1920) return 4;
    }
    return 5;
}

static void set_quality(mr_play_options *options, unsigned quality)
{
    static const unsigned widths[] = {640, 640, 854, 1280, 1920, 0};
    static const unsigned heights[] = {0, 360, 480, 720, 1080, 0};
    if (quality >= sizeof(widths) / sizeof(widths[0]))
        quality = 0;
    options->hls_low = quality == 0;
    options->hls_max_width = widths[quality];
    options->hls_max_height = heights[quality];
}

static int open_classes(void)
{
    IntuitionBase = (struct IntuitionBase *)OpenLibrary(
        (CONST_STRPTR)"intuition.library", 39);
    UtilityBase = OpenLibrary((CONST_STRPTR)"utility.library", 39);
    WindowBase = OpenLibrary((CONST_STRPTR)"window.class", YT_CLASS_VERSION);
    LayoutBase = OpenLibrary((CONST_STRPTR)"gadgets/layout.gadget",
                             YT_CLASS_VERSION);
    ButtonBase = OpenLibrary((CONST_STRPTR)"gadgets/button.gadget",
                             YT_CLASS_VERSION);
    CheckBoxBase = OpenLibrary((CONST_STRPTR)"gadgets/checkbox.gadget",
                               YT_CLASS_VERSION);
    ListBrowserBase = OpenLibrary((CONST_STRPTR)"gadgets/listbrowser.gadget",
                                  YT_CLASS_VERSION);
    StringBase = OpenLibrary((CONST_STRPTR)"gadgets/string.gadget",
                             YT_CLASS_VERSION);
    LabelBase = OpenLibrary((CONST_STRPTR)"images/label.image",
                            YT_CLASS_VERSION);
    return IntuitionBase && UtilityBase && WindowBase && LayoutBase &&
           ButtonBase && CheckBoxBase && ListBrowserBase && StringBase &&
           LabelBase;
}

static void close_classes(void)
{
#define CLOSE_BASE(base) do {                                                 \
    if (base) { CloseLibrary((struct Library *)base); base = NULL; }           \
} while (0)
    CLOSE_BASE(LabelBase);
    CLOSE_BASE(StringBase);
    CLOSE_BASE(ListBrowserBase);
    CLOSE_BASE(CheckBoxBase);
    CLOSE_BASE(ButtonBase);
    CLOSE_BASE(LayoutBase);
    CLOSE_BASE(WindowBase);
    CLOSE_BASE(UtilityBase);
    CLOSE_BASE(IntuitionBase);
#undef CLOSE_BASE
}

static void free_nodes(struct List *list)
{
    struct Node *node;
    while ((node = RemHead(list)) != NULL)
        FreeListBrowserNode(node);
}

static size_t build_nodes(struct List *list,
                          mr_youtube_search_results *results)
{
    size_t i;
    for (i = 0; i < results->count; i++) {
        struct Node *node = AllocListBrowserNode(
            1, LBNA_UserData, (ULONG)&results->items[i], LBNA_Column, 0,
            LBNCA_Text, (ULONG)results->items[i].row, TAG_END);
        if (!node)
            break;
        AddTail(list, node);
    }
    return i;
}

static void set_status(Object *status, struct Window *window, const char *text)
{
    SetGadgetAttrs((struct Gadget *)status, window, NULL,
                   STRINGA_TextVal, (ULONG)(text ? text : ""), TAG_DONE);
}

static int player_is_running(void)
{
    int running;
    Forbid();
    running = FindPort((CONST_STRPTR)MR_IPTV_PLAYER_PORT) != NULL;
    Permit();
    return running;
}

static int stop_player(void)
{
    struct MsgPort *port;
    int had_player, waited;
    Forbid();
    port = FindPort((CONST_STRPTR)MR_IPTV_PLAYER_PORT);
    if (port)
        Signal(port->mp_SigTask, SIGBREAKF_CTRL_F);
    had_player = port != NULL;
    Permit();
    if (!had_player)
        return 0;
    for (waited = 0; waited < 250 && player_is_running(); waited++)
        Delay(1);
    return 1;
}

static int start_video(const mr_youtube_search_result *video,
                       const mr_play_options *options)
{
    char watch_url[96], arguments[640];
    BPTR seglist;
    struct Process *process;

    if (!video || !video->live ||
        !mr_youtube_search_watch_url(watch_url, sizeof(watch_url), video) ||
        !mr_build_player_arguments(arguments, sizeof(arguments), options,
                                   watch_url, NULL, NULL))
        return 0;
    stop_player();
    if (player_is_running())
        return 0;
    seglist = LoadSeg((CONST_STRPTR)"PROGDIR:mrplay");
    if (!seglist)
        seglist = LoadSeg((CONST_STRPTR)"mrplay");
    if (!seglist)
        return 0;
    process = CreateNewProcTags(
        NP_Seglist, seglist,
        NP_FreeSeglist, TRUE,
        NP_Arguments, (ULONG)arguments,
        NP_StackSize, MRPLAY_STACK_SIZE,
        NP_Cli, TRUE,
        NP_CommandName, (ULONG)"mrplay",
        NP_Name, (ULONG)"MintRIVA player",
        TAG_END);
    if (!process) {
        UnLoadSeg(seglist);
        return 0;
    }
    return 1;
}

static void run_search(Object *query, Object *live_checkbox, Object *list,
                       Object *play_button, Object *status,
                       struct Window *window, struct List *nodes,
                       mr_youtube_search_results *results)
{
    static const char user_agent[] =
        "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/120.0 Safari/537.36";
    mr_http_options http_options;
    STRPTR query_text = NULL;
    ULONG live_only = TRUE;
    char search_url[1024], status_text[256];
    char *document = NULL;
    size_t document_size = 0, shown;

    GetAttr(STRINGA_TextVal, query, (ULONG *)&query_text);
    GetAttr(CHECKBOX_Checked, live_checkbox, &live_only);
    if (!mr_youtube_search_build_url(search_url, sizeof(search_url),
                                     query_text ? (char *)query_text : "",
                                     live_only != 0)) {
        set_status(status, window, mr_youtube_search_last_error());
        return;
    }
#if !defined(HAVE_AMISSL)
    set_status(status, window,
               "YouTube search requires HTTPS; rebuild with SSL=1.");
    return;
#else
    set_status(status, window, "Searching YouTube...");
    SetGadgetAttrs((struct Gadget *)play_button, window, NULL,
                   GA_Disabled, TRUE, TAG_DONE);
    Delay(1);
    if (!mr_http_options_init(&http_options, user_agent,
                              "https://www.youtube.com/") ||
        !mr_http_fetch_text(search_url, &http_options, &document,
                            &document_size, YT_SEARCH_PAGE_MAX)) {
        snprintf(status_text, sizeof(status_text), "YouTube search failed: %s",
                 mr_source_last_error());
        set_status(status, window, status_text);
        return;
    }
    SetGadgetAttrs((struct Gadget *)list, window, NULL,
                   LISTBROWSER_Labels, ~0UL, TAG_DONE);
    free_nodes(nodes);
    mr_youtube_search_results_free(results);
    if (!mr_youtube_search_parse(results, document, document_size,
                                 live_only != 0)) {
        set_status(status, window, mr_youtube_search_last_error());
        mr_free(document);
        SetGadgetAttrs((struct Gadget *)list, window, NULL,
                       LISTBROWSER_Labels, (ULONG)nodes, TAG_DONE);
        return;
    }
    mr_free(document);
    shown = build_nodes(nodes, results);
    SetGadgetAttrs((struct Gadget *)list, window, NULL,
                   LISTBROWSER_Labels, (ULONG)nodes, TAG_DONE);
    SetGadgetAttrs((struct Gadget *)play_button, window, NULL,
                   GA_Disabled, shown ? FALSE : TRUE, TAG_DONE);
    if (!shown) {
        set_status(status, window, mr_youtube_search_last_error());
    } else {
        snprintf(status_text, sizeof(status_text),
                 "%lu result%s shown%s", (unsigned long)shown,
                 shown == 1 ? "" : "s",
                 live_only ? " (live only)" : "");
        set_status(status, window, status_text);
    }
#endif
}

int main(int argc, char **argv)
{
    Object *winobj = NULL, *layout = NULL, *search_row = NULL;
    Object *query = NULL, *live_checkbox = NULL, *results_list = NULL;
    Object *status = NULL, *playback_summary = NULL;
    Object *buttons = NULL, *search_button = NULL, *play_button = NULL;
    Object *quality_button = NULL;
    Object *stop_button = NULL, *close_button = NULL, *query_label = NULL;
    struct Window *window = NULL;
    struct List result_nodes;
    mr_youtube_search_results results;
    mr_play_options play_options;
    ULONG sigmask, signals, result;
    UWORD code;
    char option_error[160], playback_text[160];
    int rc = RETURN_FAIL;
    unsigned quality_index;

    result_nodes.lh_Head = (struct Node *)&result_nodes.lh_Tail;
    result_nodes.lh_Tail = NULL;
    result_nodes.lh_TailPred = (struct Node *)&result_nodes.lh_Head;
    mr_youtube_search_results_init(&results);
    mr_play_options_default(&play_options);
    if (!mr_play_options_parse(&play_options, argc, argv, option_error,
                               sizeof(option_error))) {
        fprintf(stderr, "ytgui: %s\n", option_error);
        return RETURN_FAIL;
    }
    quality_index = quality_from_options(&play_options);
    mr_play_options_summary(&play_options, playback_text, sizeof(playback_text));
    if (!open_classes()) {
        fprintf(stderr, "ytgui: ReAction V%ld classes are not available.\n",
                (long)YT_CLASS_VERSION);
        goto cleanup;
    }

    query = (Object *)NewObject(STRING_GetClass(), NULL, GA_ID, G_QUERY,
                                GA_RelVerify, TRUE, STRINGA_MaxChars, 200,
                                TAG_DONE);
    live_checkbox = (Object *)NewObject(
        CHECKBOX_GetClass(), NULL, GA_ID, G_LIVE_ONLY,
        GA_Text, (ULONG)"Live only", CHECKBOX_Checked, TRUE,
        GA_RelVerify, TRUE, TAG_DONE);
    search_button = (Object *)NewObject(
        BUTTON_GetClass(), NULL, GA_ID, G_SEARCH, GA_Text, (ULONG)"Search",
        GA_RelVerify, TRUE, TAG_DONE);
    results_list = (Object *)NewObject(
        LISTBROWSER_GetClass(), NULL, GA_ID, G_RESULTS, GA_RelVerify, TRUE,
        LISTBROWSER_Labels, (ULONG)&result_nodes, LISTBROWSER_AutoFit, TRUE,
        LISTBROWSER_ShowSelected, TRUE, TAG_DONE);
    play_button = (Object *)NewObject(
        BUTTON_GetClass(), NULL, GA_ID, G_PLAY, GA_Text, (ULONG)"Play",
        GA_RelVerify, TRUE, GA_Disabled, TRUE, TAG_DONE);
    quality_button = (Object *)NewObject(
        BUTTON_GetClass(), NULL, GA_ID, G_QUALITY, GA_Text,
        (ULONG)quality_labels[quality_index], GA_RelVerify, TRUE, TAG_DONE);
    stop_button = (Object *)NewObject(
        BUTTON_GetClass(), NULL, GA_ID, G_STOP, GA_Text, (ULONG)"Stop",
        GA_RelVerify, TRUE, TAG_DONE);
    close_button = (Object *)NewObject(
        BUTTON_GetClass(), NULL, GA_ID, G_CLOSE, GA_Text, (ULONG)"Close",
        GA_RelVerify, TRUE, TAG_DONE);
    status = (Object *)NewObject(
        STRING_GetClass(), NULL, GA_ReadOnly, TRUE, STRINGA_TextVal,
        (ULONG)"Enter a search; public live streams are playable.",
        STRINGA_MaxChars, 256, TAG_DONE);
    playback_summary = (Object *)NewObject(
        STRING_GetClass(), NULL, GA_ReadOnly, TRUE, STRINGA_TextVal,
        (ULONG)playback_text, STRINGA_MaxChars, sizeof(playback_text), TAG_DONE);
    query_label = (Object *)NewObject(LABEL_GetClass(), NULL, LABEL_Text,
                                      (ULONG)"YouTube", TAG_DONE);
    if (!query || !live_checkbox || !search_button || !results_list ||
        !play_button || !quality_button || !stop_button || !close_button ||
        !status || !playback_summary || !query_label)
        goto cleanup;

    search_row = (Object *)NewObject(
        LAYOUT_GetClass(), NULL, LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ,
        LAYOUT_AddChild, (ULONG)query, CHILD_Label, (ULONG)query_label,
        LAYOUT_AddChild, (ULONG)live_checkbox,
        LAYOUT_AddChild, (ULONG)search_button, TAG_DONE);
    buttons = (Object *)NewObject(
        LAYOUT_GetClass(), NULL, LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ,
        LAYOUT_EvenSize, TRUE, LAYOUT_AddChild, (ULONG)play_button,
        LAYOUT_AddChild, (ULONG)quality_button,
        LAYOUT_AddChild, (ULONG)stop_button,
        LAYOUT_AddChild, (ULONG)close_button, TAG_DONE);
    if (!search_row || !buttons)
        goto cleanup;
    layout = (Object *)NewObject(
        LAYOUT_GetClass(), NULL, LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
        LAYOUT_SpaceOuter, TRUE, LAYOUT_SpaceInner, TRUE,
        LAYOUT_AddChild, (ULONG)search_row, CHILD_WeightedHeight, 0,
        LAYOUT_AddChild, (ULONG)results_list,
        LAYOUT_AddChild, (ULONG)buttons, CHILD_WeightedHeight, 0,
        LAYOUT_AddChild, (ULONG)playback_summary, CHILD_WeightedHeight, 0,
        LAYOUT_AddChild, (ULONG)status, CHILD_WeightedHeight, 0, TAG_DONE);
    if (!layout)
        goto cleanup;
    winobj = (Object *)NewObject(
        WINDOW_GetClass(), NULL, WA_Title, (ULONG)"MintRIVA YouTube",
        WA_Activate, TRUE, WA_DepthGadget, TRUE, WA_DragBar, TRUE,
        WA_CloseGadget, TRUE, WA_SizeGadget, TRUE, WA_IDCMP,
        IDCMP_GADGETUP | IDCMP_CLOSEWINDOW | IDCMP_IDCMPUPDATE,
        WINDOW_Position, WPOS_CENTERSCREEN,
        WINDOW_ParentGroup, (ULONG)layout, TAG_DONE);
    if (!winobj)
        goto cleanup;
    window = (struct Window *)RA_OpenWindow(winobj);
    if (!window)
        goto cleanup;
    GetAttr(WINDOW_SigMask, winobj, &sigmask);

    for (;;) {
        signals = Wait(sigmask | SIGBREAKF_CTRL_C);
        if (signals & SIGBREAKF_CTRL_C)
            break;
        while ((result = RA_HandleInput(winobj, &code)) != WMHI_LASTMSG) {
            ULONG gadget;
            if ((result & WMHI_CLASSMASK) == WMHI_CLOSEWINDOW)
                goto done;
            if ((result & WMHI_CLASSMASK) != WMHI_GADGETUP)
                continue;
            gadget = result & WMHI_GADGETMASK;
            if (gadget == G_CLOSE)
                goto done;
            if (gadget == G_SEARCH || gadget == G_QUERY) {
                run_search(query, live_checkbox, results_list, play_button,
                           status, window, &result_nodes, &results);
            } else if (gadget == G_STOP) {
                set_status(status, window, stop_player()
                           ? "Playback stopped." : "No video is playing.");
            } else if (gadget == G_QUALITY) {
                quality_index = (quality_index + 1) %
                    (sizeof(quality_labels) / sizeof(quality_labels[0]));
                set_quality(&play_options, quality_index);
                SetGadgetAttrs((struct Gadget *)quality_button, window, NULL,
                               GA_Text, (ULONG)quality_labels[quality_index],
                               TAG_DONE);
                mr_play_options_summary(&play_options, playback_text,
                                        sizeof(playback_text));
                SetGadgetAttrs((struct Gadget *)playback_summary, window, NULL,
                               STRINGA_TextVal, (ULONG)playback_text, TAG_DONE);
                set_status(status, window,
                           "Quality changed; applies to the next Play.");
            } else if (gadget == G_PLAY) {
                struct Node *node = NULL;
                mr_youtube_search_result *video = NULL;
                GetAttr(LISTBROWSER_SelectedNode, results_list, (ULONG *)&node);
                if (node)
                    GetListBrowserNodeAttrs(node, LBNA_UserData,
                                            (ULONG)&video, TAG_DONE);
                if (!video)
                    set_status(status, window, "Select a video first.");
                else if (!video->live)
                    set_status(status, window,
                               "Recorded YouTube videos are not supported yet.");
                else if (!start_video(video, &play_options))
                    set_status(status, window,
                               "Could not start mrplay (or old player is busy).");
                else
                    set_status(status, window, "Resolving YouTube Live...");
            }
        }
    }

done:
    rc = RETURN_OK;
cleanup:
    if (winobj) {
        if (window)
            RA_CloseWindow(winobj);
        DisposeObject(winobj);
    } else if (layout) {
        DisposeObject(layout);
    } else {
        if (search_row)
            DisposeObject(search_row);
        else {
            if (query) DisposeObject(query);
            if (live_checkbox) DisposeObject(live_checkbox);
            if (search_button) DisposeObject(search_button);
            if (query_label) DisposeObject(query_label);
        }
        if (buttons)
            DisposeObject(buttons);
        else {
            if (play_button) DisposeObject(play_button);
            if (quality_button) DisposeObject(quality_button);
            if (stop_button) DisposeObject(stop_button);
            if (close_button) DisposeObject(close_button);
        }
        if (results_list) DisposeObject(results_list);
        if (playback_summary) DisposeObject(playback_summary);
        if (status) DisposeObject(status);
    }
    free_nodes(&result_nodes);
    mr_youtube_search_results_free(&results);
    mr_http_net_shutdown();
    close_classes();
    return rc;
}
