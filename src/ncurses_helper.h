#pragma once

#include <ncurses.h>

void ncurses_init(void);
int  show_menu(const char *title, const char *footer, const char *items[], int nitems);
int  show_menu_scrollable(const char *title, const char *footer, const char *items[], int nitems);
void tui_print(WINDOW *win, int y, int x, const char *fmt, ...);
void show_message(WINDOW *win, int y, int x, const char *fmt, ...);
void show_list(WINDOW *win, const char *title, const char *footer, const char **items, int count);
void tui_getstr(WINDOW *win, int y, int x, char *buf, int maxlen);
void progress_bar(WINDOW *win, int y, int x, int items, int total_items);
