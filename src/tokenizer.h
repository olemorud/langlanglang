#pragma once

#include "file_stream.h"

#include <stdbool.h>
#include <stdint.h>

enum token_type {
    TOKEN_IDENTIFIER = 0, // [a-zA-Z][a-zA-Z0-9_]*
    TOKEN_STRING     = 1,     // "[^"]*"
    TOKEN_INTEGER    = 2,    // [0-9]+
    TOKEN_FLOATING   = 3,   // [0-9]+\.[0-9]*
    TOKEN_OPERATOR   = 4,   // + - * /
	TOKEN_EOF        = 5,
    TOKEN_UNKNOWN    = 6,
    TOKEN_TYPE_COUNT = 7
};

static const char* token_type_str[TOKEN_TYPE_COUNT] = {
    "TOKEN_IDENTIFIER",
    "TOKEN_STRING",
    "TOKEN_INTEGER",
    "TOKEN_FLOATING",
    "TOKEN_OPERATOR",
	"TOKEN_EOF",
    "TOKEN_UNKNOWN",
};

typedef struct token {
    char* start;
    char* end;
    uint64_t type;
	union {
		int64_t i;
		double  f;
	} parsed;
} Token;

Token* token_read(Error* err, Mfile* m);
void token_print(Error* err, Token* t);

int64_t token_eval_int(Error* err, Token* t);
double token_eval_float(Error* err, Token* t);


