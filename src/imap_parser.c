#include "imap_parser.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int imap_tokenize(const char *src, Token *tokens, int max) {
    int pos = 0, n = 0;
    while (n < max) {
        while (src[pos] == ' ' || src[pos] == '\r' || src[pos] == '\n' || src[pos] == '\t')
            pos++;

        char c = src[pos];
        if (c == '\0') { tokens[n++].type = TOK_EOF; break; }

        if (c == '(') { tokens[n++].type = TOK_LPAREN; pos++; continue; }
        if (c == ')') { tokens[n++].type = TOK_RPAREN; pos++; continue; }

        if (c == '"') {
            pos++;
            Token *tok = &tokens[n++];
            tok->type = TOK_STRING;
            int j = 0;
            while (src[pos] && src[pos] != '"') {
                if (src[pos] == '\\') pos++;
                if (j < (int)sizeof(tok->str) - 1)
                    tok->str[j++] = src[pos];
                pos++;
            }
            tok->str[j] = '\0';
            if (src[pos] == '"') pos++;
            continue;
        }

        if (strncmp(src + pos, "NIL", 3) == 0) {
            tokens[n++].type = TOK_NIL;
            pos += 3;
            continue;
        }

        pos++;
    }
    return n;
}

int imap_parse_envelope(const char *raw, struct ImapHeader *hdr) {
    Token tokens[1024];
    int n = imap_tokenize(raw, tokens, 1024);
    if (n < 4) return -1;

    // TODO: Parse missing from, to, cc, bcc
    hdr->date[0] = hdr->subject[0] = hdr->message_id[0] = '\0';
    int count = 0, last = -1;
    for (int i = 0; i < n; i++) {
        if (tokens[i].type != TOK_STRING) continue;
        if (count == 0) strcpy(hdr->date, tokens[i].str);
        if (count == 1) strcpy(hdr->subject, tokens[i].str);
        last = i;
        count++;
    }
    if (last != -1) strcpy(hdr->message_id, tokens[last].str);
    return count >= 3 ? 0 : -1;
}

int imap_parse_envelopes(const char *raw, struct ImapHeader *out, int max) {
    int count = 0;
    const char *p = raw;
    while (count < max) {
        p = strstr(p, "ENVELOPE (");
        if (!p) break;
        p += strlen("ENVELOPE ");

        int depth = 0;
        int in_str = 0;
        const char *q = p;
        while (*q) {
            if (in_str) {
                if (*q == '\\') q++;
                else if (*q == '"') in_str = 0;
            } else if (*q == '"') { in_str = 1; }
            else if (*q == '(')   { depth++; }
            else if (*q == ')') { depth--; if (!depth) { q++; break; } }
            q++;
        }

        size_t len = (size_t)(q - p);
        char *bounded = malloc(len + 1);
        if (!bounded) break;
        memcpy(bounded, p, len);
        bounded[len] = '\0';

        if (imap_parse_envelope(bounded, &out[count]) == 0)
            count++;
        free(bounded);
        p = q;
    }
    return count;
}
