#pragma once

#include "imap.h"

struct ParsedOpts {
    char        src_pass[256];
    char        dst_pass[256];
    const char *action;
};

int parse_opts(int argc, char *argv[], struct ParsedOpts *opts);
int parse_addr(const char *addr, struct ImapServer *srv);
int authenticate(struct ImapServer *srv, char *pass_buf, size_t pass_len);
