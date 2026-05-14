#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>

#include "imap.h"

static CURLcode transfer_mailbox(struct ImapServer src, struct ImapServer dst) {
    struct Buffer uid_buf = {0};
    CURLcode res = imap_list_uids(src, &uid_buf);
    if (res != CURLE_OK) return res;

    char *p = strstr(uid_buf.data, "SEARCH");
    if (!p) { free(uid_buf.data); return CURLE_OK; }
    p += strlen("SEARCH");

    while (*p) {
        char *end;
        long uid = strtol(p, &end, 10);
        if (end == p) break;
        p = end;

        struct Buffer msg = {0};
        res = imap_fetch_message(src, (int)uid, &msg);
        if (res == CURLE_OK)
            res = imap_append_message(dst, msg);
        free(msg.data);
        if (res != CURLE_OK) break;
    }

    free(uid_buf.data);
    return res;
}

static void extract_envelope_mid(const char *line, char *out, size_t outlen) {
    const char *end = strstr(line, ") UID");
    if (!end) { out[0] = '\0'; return; }

    const char *close = end - 1;
    while (close > line && *close != '"') close--;
    if (*close != '"') { out[0] = '\0'; return; }

    const char *open = close - 1;
    while (open > line && *open != '"') open--;
    if (*open != '"') { out[0] = '\0'; return; }

    size_t len = (size_t)(close - open - 1);
    if (len >= outlen) len = outlen - 1;
    memcpy(out, open + 1, len);
    out[len] = '\0';
}

static CURLcode validate_transfer(struct ImapServer src, struct ImapServer dst, WINDOW *win) {
    struct Buffer src_buf = {0}, dst_buf = {0};

    CURLcode res = imap_fetch_envelopes(src, &src_buf);
    if (res != CURLE_OK) {
        free(src_buf.data);
        free(dst_buf.data);
        return res;
    }

    res = imap_fetch_envelopes(dst, &dst_buf);
    if (res != CURLE_OK) {
        free(src_buf.data);
        free(dst_buf.data);
        return res;
    }

    clear();
    int row = 2, missing = 0;
    char *line = src_buf.data ? strtok(src_buf.data, "\n") : NULL;
    while (line) {
        if (strstr(line, "FETCH (ENVELOPE")) {
            char mid[256];
            extract_envelope_mid(line, mid, sizeof(mid));
            if (*mid && (!dst_buf.data || !strstr(dst_buf.data, mid))) {
                mvwprintw(win, row++, 2, "MISSING: %s", mid);
                missing++;
            }
        }
        line = strtok(NULL, "\n");
    }

    if (missing == 0)
        mvwprintw(win, row, 2, "All messages present on destination.");

    free(src_buf.data);
    free(dst_buf.data);
    return CURLE_OK;
}

static void ncurses_init(void) {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
}

static void run_action(struct ImapServer src, struct ImapServer dst, int action) {
    curl_global_init(CURL_GLOBAL_DEFAULT);

    clear();
    if (action == 0) {
        mvprintw(2, 2, "Transferring...");
        refresh();
        CURLcode res = transfer_mailbox(src, dst);
        clear();
        mvprintw(2, 2, res == CURLE_OK ? "Done. Press any key." : "Error. Press any key.");
    } else {
        mvprintw(2, 2, "Validating...");
        refresh();
        validate_transfer(src, dst, stdscr);
        mvprintw(LINES - 1, 0, "Press any key.");
    }

    curl_global_cleanup();
    refresh();
    getch();
}

static void run_tui(struct ImapServer src, struct ImapServer dst) {
    ncurses_init();

    const char *menu[] = {"Transfer all", "Validate transfer"};
    int nitems = 2;
    int selected = 0;

    int ch;
    while (1) {
        clear();
        mvprintw(LINES - 1, 0, "IMAP Mail Transfer  [up/down] Navigate  [Enter] Run  [q] Quit");
        for (int i = 0; i < nitems; i++)
            mvprintw(2 + i, 2, "%s %s", i == selected ? ">" : " ", menu[i]);
        refresh();

        ch = getch();
        if (ch == 'q') break;
        if (ch == KEY_UP && selected > 0) selected--;
        if (ch == KEY_DOWN && selected < nitems - 1) selected++;
        if (ch == '\n' || ch == KEY_ENTER)
            run_action(src, dst, selected);
    }

    endwin();
}

int main(int argc, char *argv[]) {
    if (argc != 10) {
        fprintf(stderr, "Usage: %s <src_host> <src_port> <src_user> <src_pass>"
                        " <dst_host> <dst_port> <dst_user> <dst_pass> <mailbox>\n", argv[0]);
        return 1;
    }

    struct ImapServer src = {argv[1], atoi(argv[2]), argv[3], argv[4], argv[9]};
    struct ImapServer dst = {argv[5], atoi(argv[6]), argv[7], argv[8], argv[9]};

    run_tui(src, dst);
    return 0;
}
