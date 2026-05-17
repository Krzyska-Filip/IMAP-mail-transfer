#pragma once

#include <ncurses.h>

void ncurses_init(void);
int  show_menu(const char *title, const char *footer, const char *items[], int nitems);
int  show_menu_scrollable(const char *title, const char *footer, const char *items[], int nitems);
void tui_print(WINDOW *win, int y, int x, const char *fmt, ...);
