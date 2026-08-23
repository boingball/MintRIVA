#include "mr_gui_menu.h"

#include <intuition/intuition.h>
#include <libraries/gadtools.h>
#include <proto/exec.h>
#include <proto/gadtools.h>
#include <proto/intuition.h>
#include <string.h>

struct Library *GadToolsBase;

#define MR_MENU_USER_ABOUT ((APTR)1)
#define MR_MENU_USER_QUIT  ((APTR)2)

static struct NewMenu menu_template[] = {
    {NM_TITLE, (STRPTR)"MintVID", NULL, 0, 0, NULL},
    {NM_ITEM, (STRPTR)"About MintVID...", (STRPTR)"?", 0, 0,
     MR_MENU_USER_ABOUT},
    {NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL},
    {NM_ITEM, (STRPTR)"Quit", (STRPTR)"Q", 0, 0, MR_MENU_USER_QUIT},
    {NM_END, NULL, NULL, 0, 0, NULL}
};

int mr_gui_menu_open(mr_gui_menu *menu, struct Window *window)
{
    if (!menu || !window || !window->WScreen)
        return 0;
    memset(menu, 0, sizeof(*menu));
    if (!GadToolsBase) {
        GadToolsBase = OpenLibrary((CONST_STRPTR)"gadtools.library", 37);
        menu->owns_gadtools = GadToolsBase != NULL;
    }
    if (!GadToolsBase)
        return 0;
    menu->visual_info = GetVisualInfoA(window->WScreen, NULL);
    if (!menu->visual_info)
        goto fail;
    menu->strip = CreateMenusA(menu_template, NULL);
    if (!menu->strip ||
        !LayoutMenusA(menu->strip, menu->visual_info, NULL))
        goto fail;
    SetMenuStrip(window, menu->strip);
    return 1;

fail:
    mr_gui_menu_close(menu, window);
    return 0;
}

void mr_gui_menu_close(mr_gui_menu *menu, struct Window *window)
{
    if (!menu)
        return;
    if (window && menu->strip)
        ClearMenuStrip(window);
    if (menu->strip) {
        FreeMenus(menu->strip);
        menu->strip = NULL;
    }
    if (menu->visual_info) {
        FreeVisualInfo(menu->visual_info);
        menu->visual_info = NULL;
    }
    if (GadToolsBase && menu->owns_gadtools) {
        CloseLibrary(GadToolsBase);
        GadToolsBase = NULL;
    }
    menu->owns_gadtools = 0;
}

int mr_gui_menu_action(mr_gui_menu *menu, UWORD code)
{
    struct MenuItem *item;
    APTR user_data;

    if (!menu || !menu->strip || code == MENUNULL)
        return MR_GUI_MENU_NONE;
    item = ItemAddress(menu->strip, code);
    if (!item)
        return MR_GUI_MENU_NONE;
    user_data = GTMENUITEM_USERDATA(item);
    if (user_data == MR_MENU_USER_ABOUT)
        return MR_GUI_MENU_ABOUT;
    if (user_data == MR_MENU_USER_QUIT)
        return MR_GUI_MENU_QUIT;
    return MR_GUI_MENU_NONE;
}

void mr_gui_show_about(struct Window *window, const char *edition)
{
    struct EasyStruct request;
    const char *name = edition && *edition ? edition : "AmigaOS edition";

    request.es_StructSize = sizeof(request);
    request.es_Flags = 0;
    request.es_Title = (UBYTE *)"About MintVID";
    request.es_TextFormat = (UBYTE *)
        "MintVID " MINTVID_VERSION " - Video for classic AmigaOS\n"
        "%s\n\n"
        "Made by Darren 'boingball' Banfi\n"
        "Copyright 2026\n\n"
        "Source and licence notices:\n"
        "https://github.com/boingball/MintVID\n\n"
        "Support development and the LLM token fund:\n"
        "https://buymeacoffee.com/boingball\n\n"
        "Built with libavc, libmpeg2, MintAMP audio decoders,\n"
        "liba52, Claude and Codex.";
    request.es_GadgetFormat = (UBYTE *)"Cheers!";
    EasyRequestArgs(window, &request, NULL, (APTR)&name);
}
