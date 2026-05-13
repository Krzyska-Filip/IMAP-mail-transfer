#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

typedef struct {
    char  *data;
    size_t size;
} Buffer;

static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t realsize = nmemb;
    Buffer *buf = (Buffer *)userdata;

    char *p = realloc(buf->data, buf->size + realsize + 1);
    if (!p)
        return 0; /* out of memory */

    buf->data = p;
    memcpy(&(buf->data[buf->size]), ptr, realsize);
    buf->size += realsize;
    buf->data[buf->size] = 0;

    return realsize;
}

static CURLcode imap_list_uids(const char *host, int port, const char *user, const char *pass,
                               const char *mailbox) {
    char url[512];
    snprintf(url, sizeof(url), "imap://%s:%d/%s", host, port, mailbox);

    Buffer buf = {0};
    CURL *curl = curl_easy_init();
    if (!curl) return CURLE_FAILED_INIT;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_USERNAME, user);
    curl_easy_setopt(curl, CURLOPT_PASSWORD, pass);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "UID SEARCH ALL");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK)
        fprintf(stderr, "imap_list_uids: %s\n", curl_easy_strerror(res));
    else
        printf("%s\n", buf.data ? buf.data : "");

    curl_easy_cleanup(curl);
    free(buf.data);
    return res;
}

int main(void) {
    CURLcode res = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (res != CURLE_OK) return 1;

    res = imap_list_uids("localhost", 1143, "test", "password", "INBOX");

    curl_global_cleanup();
    return (int)res;
}
