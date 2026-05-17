#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <curl/curl.h>

struct ImapServer {
    char        host[128];
    int         port;
    char        user[128];
    const char *pass;
    const char *mailbox;
    bool        ssl;
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

struct ImapHeader {
    char date[128];
    char subject[512];
    char from[256];
    char to[256];
    char in_reply_to[256];
    char message_id[256];
};

CURLcode imap_list_uids(struct ImapServer srv, struct Buffer *buf);
CURLcode imap_fetch_message(struct ImapServer srv, int uid, struct Buffer *buf);
CURLcode imap_append_message(struct ImapServer dst, struct Buffer msg);
CURLcode imap_fetch_envelopes(struct ImapServer srv, struct Buffer *buf);
CURLcode imap_delete_all(struct ImapServer srv);
void     imap_get_message_id(const char *line, char *out, size_t outlen);