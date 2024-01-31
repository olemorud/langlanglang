#pragma once

#include "file_stream.h"

#include <stdbool.h>
#include <stdint.h>

enum token_type {
    TOKEN_IDENTIFIER,    // [a-zA-Z][a-zA-Z0-9_]*
    TOKEN_STRING,        // "[^"]*"
    TOKEN_INTEGER,       // [0-9]+
    TOKEN_FLOATING,      // [0-9]+\.[0-9]*
    TOKEN_OPERATOR,      // '+' | '-' | '*' | '/'
    TOKEN_STATEMENT_END, // ';'
    TOKEN_PAREN_OPEN, 
    TOKEN_PAREN_CLOSE,
    TOKEN_IF,
    TOKEN_EOF,
    TOKEN_UNKNOWN,
    TOKEN_TYPE_COUNT
};

static const char* token_type_str[TOKEN_TYPE_COUNT] = {
    [TOKEN_IDENTIFIER]    = "TOKEN_IDENTIFIER",
    [TOKEN_STRING]        = "TOKEN_STRING",
    [TOKEN_INTEGER]       = "TOKEN_INTEGER",
    [TOKEN_FLOATING]      = "TOKEN_FLOATING",
    [TOKEN_OPERATOR]      = "TOKEN_OPERATOR",
    [TOKEN_PAREN_OPEN]    = "TOKEN_PAREN_OPEN",
    [TOKEN_PAREN_CLOSE]   = "TOKEN_PAREN_CLOSE",
    [TOKEN_IF]            = "TOKEN_IF",
    [TOKEN_STATEMENT_END] = "TOKEN_STATEMENT_END",
    [TOKEN_EOF]           = "TOKEN_EOF",
    [TOKEN_UNKNOWN]       = "TOKEN_UNKNOWN",
};

typedef struct token {
    char* start;
    char* end;
    uint32_t type;
} Token;

char* token_str(Token* t);
void token_print(Error* err, Token* t);

typedef struct token_stream {
    Token* cur;
    Mfile* m;
} TokenStream;

TokenStream tokenstream_attach(Error* err, Mfile* m);
bool        tokenstream_advance(Error* err, TokenStream* ts);
Token*      tokenstream_cur(TokenStream* ts);
Token*      tokenstream_get(Error* err, TokenStream* ts);
