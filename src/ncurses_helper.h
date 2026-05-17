#pragma once

void ncurses_init(void);
int  show_menu(const char *title, const char *footer, const char *items[], int nitems);
int  show_menu_scrollable(const char *title, const char *footer, const char *items[], int nitems);
