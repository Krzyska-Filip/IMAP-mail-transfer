#pragma once

#include "imap.h"

typedef enum {
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_STRING,
    TOK_NIL,
    TOK_EOF,
} TokenType;

typedef struct {
    TokenType type;
    char      str[512];
} Token;

int imap_tokenize(const char *src, Token *tokens, int max);
int imap_parse_envelope(const char *raw, struct ImapHeader *hdr);
int imap_parse_envelopes(const char *raw, struct ImapHeader *out, int max);
