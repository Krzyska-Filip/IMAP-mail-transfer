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

CURLcode imap_list_uids(struct ImapServer srv, struct Buffer *buf) {
    char url[512];
    snprintf(url, sizeof(url), "imap://%s:%d/%s", srv.host, srv.port, srv.mailbox);

    CURL *curl = curl_easy_init();
    if (!curl) return CURLE_FAILED_INIT;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_USERNAME, srv.user);
    curl_easy_setopt(curl, CURLOPT_PASSWORD, srv.pass);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "UID SEARCH ALL");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, buf);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK)
        fprintf(stderr, "imap_list_uids: %s\n", curl_easy_strerror(res));

    curl_easy_cleanup(curl);
    return res;
}

CURLcode imap_fetch_message(struct ImapServer srv, int uid, struct Buffer *buf) {
    char url[512];
    snprintf(url, sizeof(url), "imap://%s:%d/%s/;UID=%d", srv.host, srv.port, srv.mailbox, uid);

    CURL *curl = curl_easy_init();
    if (!curl) return CURLE_FAILED_INIT;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_USERNAME, srv.user);
    curl_easy_setopt(curl, CURLOPT_PASSWORD, srv.pass);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, buf);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK)
        fprintf(stderr, "imap_fetch_message UID=%d: %s\n", uid, curl_easy_strerror(res));

    curl_easy_cleanup(curl);
    return res;
}

CURLcode imap_append_message(struct ImapServer dst, struct Buffer msg) {
    char url[512];
    snprintf(url, sizeof(url), "imap://%s:%d/%s", dst.host, dst.port, dst.mailbox);

    CURL *curl = curl_easy_init();
    if (!curl) return CURLE_FAILED_INIT;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_USERNAME, dst.user);
    curl_easy_setopt(curl, CURLOPT_PASSWORD, dst.pass);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
    curl_easy_setopt(curl, CURLOPT_READDATA, &msg);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)msg.size);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK)
        fprintf(stderr, "imap_append_message: %s\n", curl_easy_strerror(res));

    curl_easy_cleanup(curl);
    return res;
}

CURLcode imap_fetch_envelopes(struct ImapServer srv, struct Buffer *buf) {
    char url[512];
    snprintf(url, sizeof(url), "imap://%s:%d/%s", srv.host, srv.port, srv.mailbox);

    CURL *curl = curl_easy_init();
    if (!curl) return CURLE_FAILED_INIT;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_USERNAME, srv.user);
    curl_easy_setopt(curl, CURLOPT_PASSWORD, srv.pass);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "UID FETCH 1:* (ENVELOPE)");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, buf);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK)
        fprintf(stderr, "imap_fetch_envelopes: %s\n", curl_easy_strerror(res));

    curl_easy_cleanup(curl);
    return res;
}
