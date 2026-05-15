#pragma once

#include <stddef.h>
#include <curl/curl.h>

struct ImapServer {
    const char *host;
    int         port;
    const char *user;
    const char *pass;
    const char *mailbox;
};

struct Buffer {
    char  *data;
    size_t size;
};

struct ImapRequest {
    const char    *custom_request;
    int            uid;
    struct Buffer *out;
    struct Buffer *upload;
};

CURLcode imap_list_uids(struct ImapServer srv, struct Buffer *buf);
CURLcode imap_fetch_message(struct ImapServer srv, int uid, struct Buffer *buf);
CURLcode imap_append_message(struct ImapServer dst, struct Buffer msg);
CURLcode imap_fetch_envelopes(struct ImapServer srv, struct Buffer *buf);
