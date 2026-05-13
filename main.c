#include <stdio.h>
#include <curl/curl.h>

static CURLcode imap_list_uids(const char *host, int port, const char *user, const char *pass,
                               const char *mailbox) {
    char url[512];
    snprintf(url, sizeof(url), "imap://%s:%d/%s", host, port, mailbox);

    CURL *curl = curl_easy_init();
    if (!curl) return CURLE_FAILED_INIT;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_USERNAME, user);
    curl_easy_setopt(curl, CURLOPT_PASSWORD, pass);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "UID SEARCH ALL");

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK)
        fprintf(stderr, "imap_list_uids: %s\n", curl_easy_strerror(res));

    curl_easy_cleanup(curl);
    return res;
}

int main(void) {
    CURLcode res = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (res != CURLE_OK) return 1;

    res = imap_list_uids("localhost", 1143, "test", "password", "INBOX");

    curl_global_cleanup();
    return (int)res;
}
