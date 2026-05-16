#include "init.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

int parse_addr(const char *addr, struct ImapServer *srv) {
    int matched = sscanf(addr, "%127[^@]@%127[^:]:%d",
                         srv->user, srv->host, &srv->port);

    if (matched < 2) return -1;
    if (matched == 2) srv->port = 143;
    return 0;
}

static void read_password_silent(const char *prompt, char *buf, size_t len) {
    struct termios old, noecho;
    tcgetattr(STDIN_FILENO, &old);
    noecho = old;
    noecho.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &noecho);

    fprintf(stderr, "%s", prompt);
    if (fgets(buf, (int)len, stdin))
        buf[strcspn(buf, "\n")] = '\0';

    tcsetattr(STDIN_FILENO, TCSANOW, &old);
    fprintf(stderr, "\n");
}

int authenticate(struct ImapServer *srv, char *pass_buf, size_t pass_len) {
    if (srv->pass) {
        CURLcode res = imap_list_uids(*srv, NULL);
        if (res != CURLE_OK)
            fprintf(stderr, "%s@%s: Authentication failed.\n", srv->user, srv->host);
        return res == CURLE_OK ? 0 : -1;
    }

    for (int attempt = 0; attempt < 3; attempt++) {
        char prompt[270];
        snprintf(prompt, sizeof(prompt), "%s@%s's password: ", srv->user, srv->host);
        read_password_silent(prompt, pass_buf, pass_len);
        srv->pass = pass_buf;

        CURLcode res = imap_list_uids(*srv, NULL);

        if (res == CURLE_OK) return 0;
        if (res != CURLE_LOGIN_DENIED) return -1;
        srv->pass = NULL;
        fprintf(stderr, "Permission denied, please try again.\n");
    }
    fprintf(stderr, "%s@%s: Permission denied (password).\n", srv->user, srv->host);
    return -1;
}

int parse_opts(int argc, char *argv[], struct ParsedOpts *opts) {
    *opts = (struct ParsedOpts){0};
    int opt;
    while ((opt = getopt(argc, argv, ":p:P:o:sS")) != -1) {
        switch (opt) {
            case 'p': strncpy(opts->src_pass, optarg, sizeof(opts->src_pass) - 1); break;
            case 'P': strncpy(opts->dst_pass, optarg, sizeof(opts->dst_pass) - 1); break;
            case 'o': opts->action = optarg; break;
            case 's': opts->src_ssl = true; break;
            case 'S': opts->dst_ssl = true; break;
            case ':':
                fprintf(stderr, "Option -%c requires a value\n", optopt);
                return -1;
            case '?':
                fprintf(stderr, "Unknown option: -%c\n", optopt);
                return -1;
        }
    }

    if (argc - optind != 3) {
        fprintf(stderr, "Usage: %s [-p src_pass] [-P dst_pass] [-o transfer|validate]"
                        " src_user@src_host:port dst_user@dst_host:port <mailbox>\n", argv[0]);
        return -1;
    }
    return 0;
}
