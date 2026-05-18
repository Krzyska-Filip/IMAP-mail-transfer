#include "actions.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <curl/curl.h>

#include "imap.h"
#include "imap_parser.h"
#include "ncurses_helper.h"

static CURLcode transfer_mailbox(struct ImapServer src, struct ImapServer dst,
                                 void (*on_progress)(int, int, void *), void *ctx) {
    struct Buffer uid_buf = {0};
    CURLcode res = imap_list_uids(src, &uid_buf);
    if (res != CURLE_OK) return res;

    char *p = strstr(uid_buf.data, "SEARCH");
    if (!p) { free(uid_buf.data); return CURLE_OK; }
    p += strlen("SEARCH");

    int total = 0;
    for (char *q = p; *q;) {
        char *end;
        strtol(q, &end, 10);
        if (end == q) break;
        total++; q = end;
    }

    int current = 0;
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
        if (on_progress) on_progress(++current, total, ctx);
        if (res != CURLE_OK) break;
    }

    free(uid_buf.data);
    return res;
}

static void on_transfer_progress(int current, int total, void *ctx) {
    progress_bar((WINDOW *)ctx, 4, 2, current, total);
}

void action_transfer(struct ImapServer src, struct ImapServer dst, WINDOW *win) {
    if (win) {
        char fwd[300], rev[300];
        snprintf(fwd, sizeof(fwd), "%s@%s/%s -> %s@%s/%s", src.user, src.host, src.mailbox, dst.user, dst.host, dst.mailbox);
        snprintf(rev, sizeof(rev), "%s@%s/%s <- %s@%s/%s", src.user, src.host, src.mailbox, dst.user, dst.host, dst.mailbox);
        const char *items[] = { fwd, rev };
        int sel = show_menu("Transfer", "Transfer  [up/down] Navigate  [Enter] Select  [q] Cancel", items, 2);
        if (sel < 0) return;
        if (sel == 1) { struct ImapServer tmp = src; src = dst; dst = tmp; }
        clear();
    }

    tui_print(win, 2, 2, "Transfering...");

    CURLcode res = transfer_mailbox(src, dst, on_transfer_progress, win);
    if (win)
        clear();
    else
        printf("\n");

    if (res == CURLE_OK)
        show_message(win, 2, 2, "Done.");
    else
        show_message(win, 2, 2, "Error: %s", curl_easy_strerror(res));
}

void action_validate(struct ImapServer src, struct ImapServer dst, WINDOW *win) {
    if (win) {
        char fwd[300], rev[300];
        snprintf(fwd, sizeof(fwd), "%s@%s/%s -> %s@%s/%s", src.user, src.host, src.mailbox, dst.user, dst.host, dst.mailbox);
        snprintf(rev, sizeof(rev), "%s@%s/%s <- %s@%s/%s", src.user, src.host, src.mailbox, dst.user, dst.host, dst.mailbox);
        const char *items[] = { fwd, rev };
        int sel = show_menu("Validate", "Validate  [up/down] Navigate  [Enter] Select  [q] Cancel", items, 2);
        if (sel < 0) return;
        if (sel == 1) { struct ImapServer tmp = src; src = dst; dst = tmp; }
        clear();
    }
    
    tui_print(win, 2, 2, "Validating...");

    struct Buffer src_buf = {0}, dst_buf = {0};

    CURLcode res = imap_fetch_envelopes(src, &src_buf);
    if (res != CURLE_OK) {
        free(src_buf.data);
        return;
    }

    res = imap_fetch_envelopes(dst, &dst_buf);
    if (res != CURLE_OK) {
        free(src_buf.data); free(dst_buf.data);
        return;
    }

    struct ImapHeader *src_hdrs = malloc(8192 * sizeof(struct ImapHeader));
    struct ImapHeader *dst_hdrs = malloc(8192 * sizeof(struct ImapHeader));
    if (!src_hdrs || !dst_hdrs) {
        free(src_hdrs); free(dst_hdrs);
        free(src_buf.data); free(dst_buf.data);
        return;
    }

    int src_n = imap_parse_envelopes(src_buf.data ? src_buf.data : "", src_hdrs, 8192);
    int dst_n = imap_parse_envelopes(dst_buf.data ? dst_buf.data : "", dst_hdrs, 8192);

    int missing = 0;
    int id_width = 0;
    int *missing_idx = malloc(src_n * sizeof(int));
    char **missing_lines = malloc(src_n * sizeof(char *));
    if (!missing_idx || !missing_lines) {
        free(missing_idx); free(missing_lines);
        free(src_hdrs); free(dst_hdrs);
        free(src_buf.data); free(dst_buf.data);
        return;
    }

    // TODO: O(n) = n^2 -> sort before by message_id
    for (int i = 0; i < src_n; i++) {
        int found = 0;
        for (int j = 0; j < dst_n; j++) {
            if (strcmp(src_hdrs[i].message_id, dst_hdrs[j].message_id) == 0) {
                found = 1; break;
            }
        }
        if (!found) {
            missing_idx[missing++] = i;
            int len = (int)strlen(src_hdrs[i].message_id);
            if (len > id_width) id_width = len;
        }
    }
    if (id_width > 255) id_width = 255;

    for (int k = 0; k < missing; k++) {
        int i = missing_idx[k];
        char *line = malloc(768);
        if (line) {
            char subj_buf[129];
            if (strlen(src_hdrs[i].subject) > 128)
                snprintf(subj_buf, sizeof(subj_buf), "%.123s[...]", src_hdrs[i].subject);
            else
                snprintf(subj_buf, sizeof(subj_buf), "%.128s", src_hdrs[i].subject);

            int n = snprintf(line, 768, "%s", src_hdrs[i].message_id);
            if (n < id_width) { memset(line + n, ' ', id_width - n); n = id_width; }
            snprintf(line + n, 768 - n, "  %s", subj_buf);
            missing_lines[k] = line;
        }
    }
    free(missing_idx);

    char title[64];
    if (missing == 0)
        snprintf(title, sizeof(title), "All %d messages present.", src_n);
    else
        snprintf(title, sizeof(title), "Missing %d", missing);

    const char* menu_footer = "Missing messages  [up/down] Scroll  [q] Back";
    show_list(win, title, menu_footer, (const char **)missing_lines, missing);

    for (int i = 0; i < missing; i++) free(missing_lines[i]);
    free(missing_lines);
    free(src_hdrs); free(dst_hdrs);
    free(src_buf.data); free(dst_buf.data);
}

void action_clear(struct ImapServer src, struct ImapServer dst, WINDOW *win) {
    char src_item[300], dst_item[300];
    snprintf(src_item, sizeof(src_item), "Source      %s@%s/%s", src.user, src.host, src.mailbox);
    snprintf(dst_item, sizeof(dst_item), "Destination %s@%s/%s", dst.user, dst.host, dst.mailbox);
    const char *items[] = { src_item, dst_item };

    char *menu_header = "Clear Mailbox";
    char *menu_footer = "Clear mailbox  [up/down] Navigate  [Enter] Select  [q] Cancel";
    int sel = show_menu(menu_header, menu_footer, items, 2);
    if (sel < 0) return;

    struct ImapServer target = sel == 0 ? src : dst;

    clear();
    mvprintw(0, 2, "Clear mailbox");
    mvprintw(2, 2, "Clear %s@%s/%s?", target.user, target.host, target.mailbox);
    mvprintw(4, 2, "Type 'confirm' to proceed: ");
    mvprintw(LINES - 1, 0, "Clear mailbox  [confirm] Execute  [q/Enter] Cancel");
    refresh();

    char input[16];
    tui_getstr(win, 4, 29, input, (int)sizeof(input) - 1);

    if (strcmp(input, "confirm") != 0) {
        show_message(win, 2, 2, "Cancelled.");
        return;
    }

    tui_print(stdscr, 6, 2, "Clearing...");
    CURLcode res = imap_delete_all(target);
    clear();

    if (res == CURLE_OK)
        show_message(win, 2, 2, "Done.");
    else
        show_message(win, 2, 2, "Error: %s", curl_easy_strerror(res));
}

void run_action(struct ImapServer src, struct ImapServer dst, int action) {
    clear();
    switch (action) {
        case 0:
            action_transfer(src, dst, stdscr);
            break;
        case 1:
            action_validate(src, dst, stdscr);
            break;
        case 2:
            action_clear(src, dst, stdscr);
            break;
        default:
            tui_print(stdscr, 2, 2, "Action not implemented");
            break;
    }
    refresh();
}
