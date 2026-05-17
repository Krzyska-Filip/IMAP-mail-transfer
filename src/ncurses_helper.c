#include "ncurses_helper.h"

#include <ncurses.h>

void ncurses_init(void) {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
}

int show_menu(const char *title, const char *footer, const char *items[], int nitems) {
    int sel = 0, ch;
    while (1) {
        clear();
        mvprintw(0, 2, "%s", title);
        mvprintw(LINES - 1, 0, "%s", footer);
        for (int i = 0; i < nitems; i++)
            mvprintw(2 + i, 2, "%s %s", i == sel ? ">" : " ", items[i]);
        refresh();

        ch = getch();
        if (ch == 'q')                        return -1;
        if (ch == KEY_UP   && sel > 0)        sel--;
        if (ch == KEY_DOWN && sel < nitems-1) sel++;
        if (ch == '\n' || ch == KEY_ENTER)    return sel;
    }
}

int show_menu_scrollable(const char *title, const char *footer, const char *items[], int nitems) {
    int sel = 0, offset = 0, ch;
    const int visible = 10;
    while (1) {
        clear();
        mvprintw(0, 2, "%s", title);
        mvprintw(LINES - 1, 0, "%s", footer);
        for (int i = 0; i < visible && (offset + i) < nitems; i++) {
            int idx = offset + i;
            mvprintw(2 + i, 2, "%s %s", idx == sel ? ">" : " ", items[idx]);
        }
        refresh();

        ch = getch();
        if (ch == 'q') {
            return -1;
        }
        if (ch == KEY_UP && sel > 0) {
            sel--;
            if (sel < offset) offset--;
        }
        if (ch == KEY_DOWN && sel < nitems - 1) {
            sel++;
            if (sel >= offset + visible) offset++;
        }
        if (ch == '\n' || ch == KEY_ENTER) {
            return sel;
        }
    }
}
