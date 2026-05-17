#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ncurses.h>

#include "imap.h"
#include "imap_parser.h"
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
        return res;
    }

    res = imap_fetch_envelopes(dst, &dst_buf);
    if (res != CURLE_OK) {
        free(src_buf.data); free(dst_buf.data);
        return res;
    }

    struct ImapHeader *src_hdrs = malloc(8192 * sizeof(struct ImapHeader));
    struct ImapHeader *dst_hdrs = malloc(8192 * sizeof(struct ImapHeader));
    if (!src_hdrs || !dst_hdrs) {
        free(src_hdrs); free(src_buf.data);
        free(dst_hdrs); free(dst_buf.data);
        return CURLE_OUT_OF_MEMORY;
    }

    int src_n = imap_parse_envelopes(src_buf.data ? src_buf.data : "", src_hdrs, 8192);
    int dst_n = imap_parse_envelopes(dst_buf.data ? dst_buf.data : "", dst_hdrs, 8192);

    int missing = 0;
    char **missing_lines = malloc(src_n * sizeof(char *));
    if (!missing_lines) {
        free(src_hdrs); free(dst_hdrs);
        free(src_buf.data); free(dst_buf.data);
        return CURLE_OUT_OF_MEMORY;
    }

    int id_width = 0;
    int *missing_idx = malloc(src_n * sizeof(int));
    if (!missing_idx) {
        free(src_hdrs); free(dst_hdrs);
        free(src_buf.data); free(dst_buf.data);
        free(missing_lines);
        return CURLE_OUT_OF_MEMORY;
    }

    // TODO: O(n) = n^2 -> sort before by message_id
    for (int i = 0; i < src_n; i++) {
        int found = 0;
        for (int j = 0; j < dst_n; j++) {
            if (strcmp(src_hdrs[i].message_id, dst_hdrs[j].message_id) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) {
            missing_idx[missing++] = i;
            int len = (int)strlen(src_hdrs[i].message_id);
            if (len > id_width) id_width = len;
        }
    }

    for (int k = 0; k < missing; k++) {
        int i = missing_idx[k];
        char *line = malloc(768);
        if (line) {
            const char *subj = src_hdrs[i].subject;
            char subj_buf[129];
            if (strlen(subj) > 128) {
                snprintf(subj_buf, sizeof(subj_buf), "%.123s[...]", subj);
                subj = subj_buf;
            }
            snprintf(line, 768, "%-*s  %s", id_width, src_hdrs[i].message_id, subj);
            missing_lines[k] = line;
        }
    }
    free(missing_idx);

    char title[64];
    if (missing == 0)
        snprintf(title, sizeof(title), "All %d messages present.", src_n);
    else
        snprintf(title, sizeof(title), "Missing %d", missing);

    const char *menu_footer = "Missing messages  [up/down] Scroll  [q] Back";
    show_list(win, title, menu_footer, (const char **)missing_lines, missing);

    for (int i = 0; i < missing; i++) free(missing_lines[i]);
    free(missing_lines);
    free(src_hdrs);
    free(dst_hdrs);
    free(src_buf.data);
    free(dst_buf.data);
    return CURLE_OK;
}


static void run_action(struct ImapServer src, struct ImapServer dst, int action) {
    clear();
    if (action == 0) {
        tui_print(stdscr, 2, 2, "Transfering...");
        CURLcode res = transfer_mailbox(src, dst);
        clear();
        if (res == CURLE_OK)
            show_message(stdscr, 2, 2, "Done.");
        else
            show_message(stdscr, 2, 2, "Error: %s", curl_easy_strerror(res));
    } else if (action == 1) {
        tui_print(stdscr, 2, 2, "Validating...");
        validate_transfer(src, dst, stdscr);
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
            show_message(stdscr, 2, 2, "Cancelled");
            return;
        }

        tui_print(stdscr, 6, 2, "Clearing...");
        CURLcode res = imap_delete_all(target);

        clear();

        if (res == CURLE_OK)
            show_message(stdscr, 2, 2, "Done", curl_easy_strerror(res));
        else
            show_message(stdscr, 2, 2, "Error: %s", curl_easy_strerror(res));
    }

    refresh();
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