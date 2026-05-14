#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <ncurses.h>

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

static CURLcode imap_list_uids(struct ImapServer srv, struct Buffer *buf) {
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

static CURLcode imap_fetch_message(struct ImapServer srv, int uid, struct Buffer *buf) {
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

static CURLcode imap_append_message(struct ImapServer dst, struct Buffer msg) {
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

static CURLcode transfer_mailbox(struct ImapServer src, struct ImapServer dst) {
    struct Buffer uid_buf = {0};
    CURLcode res = imap_list_uids(src, &uid_buf);
    if (res != CURLE_OK) return res;

    char *p = strstr(uid_buf.data, "SEARCH");
    if (!p) { free(uid_buf.data); return CURLE_OK; }
    p += strlen("SEARCH");

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
        if (res != CURLE_OK) break;
    }

    free(uid_buf.data);
    return res;
}

static void run_tui(struct ImapServer src, struct ImapServer dst) {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    const char *menu[] = {"Transfer all"};
    int selected = 0;

    int ch;
    while (1) {
        clear();
        mvprintw(0, 0, "IMAP Mail Transfer  [Enter] Run  [q] Quit");
        mvprintw(2, 2, "%s %s", selected == 0 ? ">" : " ", menu[0]);
        refresh();

        ch = getch();
        if (ch == 'q') break;
        if (ch == '\n' || ch == KEY_ENTER) {
            endwin();
            curl_global_init(CURL_GLOBAL_DEFAULT);
            CURLcode res = transfer_mailbox(src, dst);
            curl_global_cleanup();
            initscr();
            cbreak();
            noecho();
            keypad(stdscr, TRUE);
            curs_set(0);
            clear();
            mvprintw(0, 0, res == CURLE_OK ? "Done. Press any key." : "Error. Press any key.");
            refresh();
            getch();
            break;
        }
    }

    endwin();
}

int main(int argc, char *argv[]) {
    if (argc != 10) {
        fprintf(stderr, "Usage: %s <src_host> <src_port> <src_user> <src_pass>"
                        " <dst_host> <dst_port> <dst_user> <dst_pass> <mailbox>\n", argv[0]);
        return 1;
    }

    struct ImapServer src = {argv[1], atoi(argv[2]), argv[3], argv[4], argv[9]};
    struct ImapServer dst = {argv[5], atoi(argv[6]), argv[7], argv[8], argv[9]};

    run_tui(src, dst);
    return 0;
}
