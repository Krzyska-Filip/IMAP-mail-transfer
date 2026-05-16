#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ncurses.h>

#include "imap.h"
#include "init.h"
#include "ncurses_helper.h"

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

    if (win) clear();
    int row = 2, missing = 0;
    char *line = src_buf.data ? strtok(src_buf.data, "\n") : NULL;
    while (line) {
        if (strstr(line, "FETCH (ENVELOPE")) {
            char mid[256];
            imap_get_message_id(line, mid, sizeof(mid));
            if (*mid && (!dst_buf.data || !strstr(dst_buf.data, mid))) {
                if (win) mvwprintw(win, row++, 2, "MISSING: %s", mid);
                else printf("MISSING: %s\n", mid);
                missing++;
            }
        }
        line = strtok(NULL, "\n");
    }

    if (missing == 0) {
        if (win) mvwprintw(win, row, 2, "All messages present on destination.");
        else printf("All messages present on destination.\n");
    }

    free(src_buf.data);
    free(dst_buf.data);
    return CURLE_OK;
}


static void run_action(struct ImapServer src, struct ImapServer dst, int action) {
    clear();
    if (action == 0) {
        mvprintw(2, 2, "Transferring...");
        refresh();
        CURLcode res = transfer_mailbox(src, dst);
        clear();
        if (res == CURLE_OK)
            mvprintw(2, 2, "Done. Press any key.");
        else
            mvprintw(2, 2, "Error: %s. Press any key.", curl_easy_strerror(res));
    } else if (action == 1) {
        mvprintw(2, 2, "Validating...");
        refresh();
        validate_transfer(src, dst, stdscr);
        mvprintw(LINES - 1, 0, "Press any key.");
    } else {
        char src_item[300], dst_item[300];
        snprintf(src_item, sizeof(src_item), "Source      %s@%s/%s", src.user, src.host, src.mailbox);
        snprintf(dst_item, sizeof(dst_item), "Destination %s@%s/%s", dst.user, dst.host, dst.mailbox);
        const char *items[] = { src_item, dst_item };
        const char *menu_title = "Clear Mailbox";
        const char *menu_footer = "Clear mailbox  [up/down] Navigate  [Enter] Select  [q] Cancel";

        int sel = show_menu(menu_title, menu_footer, items, 2);
        if (sel < 0) return;

        struct ImapServer target = sel == 0 ? src : dst;

        clear();
        mvprintw(0, 2, "Clear mailbox");
        mvprintw(2, 2, "Clear %s@%s/%s?", target.user, target.host, target.mailbox);
        mvprintw(4, 2, "Type 'confirm' to proceed: ");
        mvprintw(LINES - 1, 0, "Clear mailbox  [confirm] Execute  [q/Enter] Cancel");
        refresh();

        echo();
        curs_set(1);
        char input[16];
        mvgetnstr(4, 29, input, (int)sizeof(input) - 1);
        noecho();
        curs_set(0);

        if (strcmp(input, "confirm") != 0) {
            mvprintw(6, 2, "Cancelled. Press any key.");
            refresh();
            getch();
            return;
        }

        mvprintw(6, 2, "Clearing...");
        refresh();
        CURLcode res = imap_delete_all(target);
        clear();
        if (res == CURLE_OK)
            mvprintw(2, 2, "Done. Press any key.");
        else
            mvprintw(2, 2, "Error: %s. Press any key.", curl_easy_strerror(res));
    }

    refresh();
    getch();
}

static void run_tui(struct ImapServer src, struct ImapServer dst) {
    ncurses_init();

    const char *items[] = {"Transfer all", "Validate transfer", "Clear mailbox"};
    const char *menu_title = "IMAP Mail Transfer";
    const char *menu_footer = "IMAP Mail Transfer  [up/down] Navigate  [Enter] Run  [q] Quit";
    int sel;
    while ((sel = show_menu(menu_title, menu_footer, items, 3)) != -1)
        run_action(src, dst, sel);

    endwin();
}

static void run_cli(struct ImapServer src, struct ImapServer dst, const char *action) {
    if (strcmp(action, "transfer") == 0) {
        printf("Transferring...\n");
        CURLcode res = transfer_mailbox(src, dst);
        if (res == CURLE_OK)
            printf("Done.\n");
        else
            fprintf(stderr, "Error: %s\n", curl_easy_strerror(res));
    } else if (strcmp(action, "validate") == 0) {
        printf("Validating...\n");
        validate_transfer(src, dst, NULL);
    } else {
        fprintf(stderr, "Unknown action: %s\n", action);
    }
}


int main(int argc, char *argv[]) {
    struct ImapServer src, dst;
    struct ParsedOpts opts;

    int parse_result = parse_opts(argc, argv, &opts);
    if (parse_result != 0) {
        return 1;
    }

    const char* src_addr = argv[optind];
    const char* dst_addr = argv[optind + 1];

    src.mailbox = argv[optind + 2];
    dst.mailbox = argv[optind + 2];

    src.pass = opts.src_pass[0] ? opts.src_pass : NULL;
    dst.pass = opts.dst_pass[0] ? opts.dst_pass : NULL;
    src.ssl  = opts.src_ssl;
    dst.ssl  = opts.dst_ssl;

    if (parse_addr(src_addr, &src) != 0) {
        fprintf(stderr, "Invalid source address (expected user@host:port)\n");
        return 1;
    }

    if (parse_addr(dst_addr, &dst) != 0) {
        fprintf(stderr, "Invalid destination address (expected user@host:port)\n");
        return 1;
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);

    if (authenticate(&src, opts.src_pass, sizeof(opts.src_pass)) != 0 ||
        authenticate(&dst, opts.dst_pass, sizeof(opts.dst_pass)) != 0) {
        curl_global_cleanup();
        return 1;
    }

    if (opts.action)
        run_cli(src, dst, opts.action);
    else
        run_tui(src, dst);

    curl_global_cleanup();
    return 0;
}