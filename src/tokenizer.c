
#include "error.h"
#include "file_stream.h"
#include "tokenizer.h"
#include "printable.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

static const bool is_operator[256] = {
    ['+'] = 1, ['-'] = 1, ['*'] = 1, ['/'] = 1, ['='] = 1, ['%'] = 1,
    ['&'] = 1, ['|'] = 1, ['<'] = 1, ['>'] = 1, ['!'] = 1, ['^'] = 1,
    ['('] = 1, [')'] = 1, ['~'] = 1
};

static void token_read_operator(Error* err, Mfile* m, Token* t)
{
    (void)err;
    t->type = TOKEN_OPERATOR;
    t->start = mfile_cur(m);
    while (!mfile_eof(m) && is_operator[(unsigned char)mfile_get(m)])
        /* NOOP */;
    mfile_decr_pos(m);
    t->end = mfile_cur(m);
}

static void token_read_number(Error* err, Mfile* m, Token* t)
{
    (void)err;
    t->start = mfile_cur(m);

    if (mfile_curchar(m) == '-') {
        mfile_inc_pos(m);
    }
    mfile_skip(m, isdigit);
    if (mfile_curchar(m) == '.') {
        t->type = TOKEN_FLOATING;
        mfile_inc_pos(m);
        mfile_skip(m, isdigit);
    } else {
        t->type = TOKEN_INTEGER;
    }
    t->end = mfile_cur(m);
}

static void token_read_string(Error* err, Mfile* m, Token* t)
{
    t->type = TOKEN_STRING;
    t->start = mfile_cur(m);

    mfile_inc_pos(m);
    bool escaped = false;
    while (!mfile_eof(m) && (mfile_curchar(m) != '"' || escaped)) {
        escaped = mfile_get(m) == '\\';
    }
    if (mfile_curchar(m) != '"') {
        error_push(err, "expected '\"', got %c", PRINTABLE(*(mfile_cur(m))));
        return;
    }
    mfile_inc_pos(m);

    t->end = mfile_cur(m);
}

static void token_read_keyword_or_identifier(Error* err, Mfile* m, Token* t)
{
    (void)err;
    t->start = mfile_cur(m);

    assert(isalpha(*(t->start)));

    mfile_skip(m, isalnum);

    t->end = mfile_cur(m);

#define IS_KEYWORD(s) \
    (memcmp(t->start, s, \
            MIN((ssize_t)(sizeof(s)-1), (ssize_t)(t->end - t->start))) == 0)
    if (IS_KEYWORD("if")) {
        t->type = TOKEN_IF;
    } else if (IS_KEYWORD("while")) {
        fprintf(stderr, "while statements not implemented\n");
        exit(1);
    } else {
        t->type = TOKEN_IDENTIFIER;
    }
#undef IS_KEYWORD
}

Token* token_read(Error* err, Mfile* m)
{
    Token* t = calloc(1, sizeof *t);
    if (!t) {
        error_push(err, "failed to allocate token: %s", strerror(errno));
        return NULL;
    }

    mfile_skip(m, isspace);
    const int c = mfile_curchar(m);

    if (isalpha(c)) {
        token_read_keyword_or_identifier(err, m, t);
    } else if (c == '"') {
        token_read_string(err, m, t);
    } else if (c == ';') {
        t->type = TOKEN_STATEMENT_END;
        t->start = mfile_cur(m);
        mfile_inc_pos(m);
        t->end = mfile_cur(m);
    } else if (c == '(') {
        t->type = TOKEN_PAREN_OPEN;
        t->start = mfile_cur(m);
        mfile_inc_pos(m);
        t->end = mfile_cur(m);
    } else if (c == ')') {
        t->type = TOKEN_PAREN_CLOSE;
        t->start = mfile_cur(m);
        mfile_inc_pos(m);
        t->end = mfile_cur(m);
    } else if (isdigit(c)) {  // signs are handled by parser.c
        token_read_number(err, m, t);
    } else if (is_operator[c]) {
        token_read_operator(err, m, t);
    } else if (c == EOF) {
        t->type = TOKEN_EOF;
    } else {
        error_push(err, "unexpected character: %s (0x%02x)", PRINTABLE(c), c);
    }

    return t;
}

void token_print(Error* err, Token* t)
{
    int ok;
    char* start = t->start;
    ok = fprintf(stderr, "[%s:%d \"", token_type_str[t->type], t->type);
    if (ok < 0) {
        error_push(err, "failed to print token: %s", strerror(errno));
        return;
    }
    while (start < t->end) {
        ok = fputc(*start, stderr);
        if (ok < 0) {
            error_push(err, "failed to print token: %s", strerror(errno));
            return;
        }
        start += 1;
    }
    fprintf(stderr, "\"]\n");
}

char* token_str(Token* t)
{
    static __thread char buf[512];
    memcpy(buf, t->start, t->end - t->start);
    buf[t->start - t->end] = '\0';
    return buf;
}

bool tokenstream_advance(Error* err, TokenStream* ts)
{
    ts->cur = token_read(err, ts->m);
    if (!error_empty(err)) {
        error_push(err, "failed");
        return false;
    }
    return true;
}

TokenStream tokenstream_attach(Error* err, Mfile* m)
{
    TokenStream ts = {.cur = NULL, .m = m};
    ts.cur = token_read(err, m);
    return ts;
}

Token* tokenstream_cur(TokenStream* ts)
{
    return ts->cur;
}

Token* tokenstream_get(Error* err, TokenStream* ts)
{
    Token* cur = tokenstream_cur(ts);
    tokenstream_advance(err, ts);
    return cur;
}
