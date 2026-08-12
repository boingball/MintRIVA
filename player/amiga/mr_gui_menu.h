#ifndef MR_GUI_MENU_H
#define MR_GUI_MENU_H

#include <exec/types.h>

struct Menu;
struct Window;

typedef struct mr_gui_menu {
    struct Menu *strip;
    APTR visual_info;
    int owns_gadtools;
} mr_gui_menu;

enum {
    MR_GUI_MENU_NONE = 0,
    MR_GUI_MENU_ABOUT,
    MR_GUI_MENU_QUIT
};

int mr_gui_menu_open(mr_gui_menu *menu, struct Window *window);
void mr_gui_menu_close(mr_gui_menu *menu, struct Window *window);
int mr_gui_menu_action(mr_gui_menu *menu, UWORD code);
void mr_gui_show_about(struct Window *window, const char *edition);

#endif
