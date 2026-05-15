#include "imap.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t realsize = nmemb;
    struct Buffer *buf = (struct Buffer *)userdata;

    char *p = realloc(buf->data, buf->size + realsize + 1);
    if (!p)
        return 0; /* out of memory */

    buf->data = p;
    memcpy(&(buf->data[buf->size]), ptr, realsize);
    buf->size += realsize;
    buf->data[buf->size] = 0;

    return realsize;
}

static size_t read_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    struct Buffer *buf = (struct Buffer *)userdata;
    size_t to_copy = buf->size < size * nmemb ? buf->size : size * nmemb;
    memcpy(ptr, buf->data, to_copy);
    buf->data += to_copy;
    buf->size -= to_copy;
    return to_copy;
}

static CURLcode imap_execute(struct ImapServer srv, struct ImapRequest req, const char *label) {
    char url[512];
    if (req.uid > 0)
        snprintf(url, sizeof(url), "imap://%s:%d/%s/;UID=%d", srv.host, srv.port, srv.mailbox, req.uid);
    else
        snprintf(url, sizeof(url), "imap://%s:%d/%s", srv.host, srv.port, srv.mailbox);

    CURL *curl = curl_easy_init();
    if (!curl) return CURLE_FAILED_INIT;
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_USERNAME, srv.user);
    curl_easy_setopt(curl, CURLOPT_PASSWORD, srv.pass);
    if (req.out) {
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, req.out);
    }
    if (req.upload) {
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
        curl_easy_setopt(curl, CURLOPT_READDATA, req.upload);
        curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)req.upload->size);
    }
    if (req.custom_request)
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, req.custom_request);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK)
        fprintf(stderr, "%s: %s\n", label, curl_easy_strerror(res));
    curl_easy_cleanup(curl);
    return res;
}

CURLcode imap_list_uids(struct ImapServer srv, struct Buffer *buf) {
    struct ImapRequest req = {0};
    req.custom_request = "UID SEARCH ALL";
    req.out = buf;
    return imap_execute(srv, req, "imap_list_uids");
}

CURLcode imap_fetch_message(struct ImapServer srv, int uid, struct Buffer *buf) {
    struct ImapRequest req = {0};
    req.uid = uid;
    req.out = buf;
    return imap_execute(srv, req, "imap_fetch_message");
}

CURLcode imap_append_message(struct ImapServer dst, struct Buffer msg) {
    struct ImapRequest req = {0};
    req.upload = &msg;
    return imap_execute(dst, req, "imap_append_message");
}

CURLcode imap_fetch_envelopes(struct ImapServer srv, struct Buffer *buf) {
    struct ImapRequest req = {0};
    req.custom_request = "UID FETCH 1:* (ENVELOPE)";
    req.out = buf;
    return imap_execute(srv, req, "imap_fetch_envelopes");
}
