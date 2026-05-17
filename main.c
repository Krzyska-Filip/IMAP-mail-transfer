#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "imap.h"
#include "init.h"
#include "ncurses_helper.h"
#include "actions.h"

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
    if (strcmp(action, "transfer") == 0)
        action_transfer(src, dst, NULL);
    else if (strcmp(action, "validate") == 0)
        action_validate(src, dst, NULL);
    else
        fprintf(stderr, "Unknown action: %s\n", action);
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