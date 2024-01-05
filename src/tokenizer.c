
#include "error.h"
#include "file_stream.h"
#include "tokenizer.h"
#include "printable.h"

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
    t->type = TOKEN_OPERATOR;
    t->start = mfile_cur(m);
    while (!mfile_eof(m) && is_operator[(unsigned char)mfile_get(m)])
        /* NOOP */;
    m->pos -= 1;
    t->end = mfile_cur(m);
}

static void token_read_number(Error* err, Mfile* m, Token* t)
{
    t->start = mfile_cur(m);

	if (mfile_curchar(m) == '-') {
		m->pos += 1;
	}

    mfile_skip(m, isdigit);

    if (mfile_curchar(m) == '.') {
        t->type = TOKEN_FLOATING;
        m->pos += 1;
        mfile_skip(m, isdigit);
    } else {
        t->type = TOKEN_INTEGER;
    }
    t->end = mfile_cur(m);
}

int64_t token_eval_int(Error* err, Token* t)
{
	char* endptr;
	int64_t n = strtoll(t->start, &endptr, 10);
	if (endptr != t->end) {
		error_push(err, "%s: invalid integer: %s", __func__, t->start);
		return 0;
	}
	return n;
}

double token_eval_float(Error* err, Token* t)
{
	char* endptr;
	double n = strtod(t->start, &endptr);
	if (endptr != t->end) {
		error_push(err, "%s: invalid floating point: %s", __func__, t->start);
		return 0;
	}
	if (n == HUGE_VAL || n == -HUGE_VAL) {
		error_push(err, "%s: invalid floating point: %s", __func__, t->start);
	}
	return n;
}

static void token_read_string(Error* err, Mfile* m, Token* t)
{
    t->type = TOKEN_STRING;
    t->start = mfile_cur(m);

    m->pos += 1;
    bool escaped = false;
    while (!mfile_eof(m) && (mfile_curchar(m) != '"' || escaped)) {
        escaped = mfile_get(m) == '\\';
    }
    if (mfile_curchar(m) != '"') {
        error_push(err, "%s: expected '\"', got %c", __func__, PRINTABLE(*(mfile_cur(m))));
        return;
    }
    m->pos += 1;

    t->end = mfile_cur(m);
}

static void token_read_identifier(Error* err, Mfile* m, Token* t)
{
    t->type = TOKEN_IDENTIFIER;
    t->start = mfile_cur(m);

    if (!isalpha(*(t->start))) {
        error_push(err, "%s, expected identifier, got %c", __func__, PRINTABLE(*(t->start)));
        return;
    }

    mfile_skip(m, isalnum);

    t->end = mfile_cur(m);

    return;
}

static void token_eval_identifier(Token* t)
{
	
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
        token_read_identifier(err, m, t);
    } else if (c == '"') {
        token_read_string(err, m, t);
    } else if (isdigit(c) || c == '-') {
        token_read_number(err, m, t);
    } else if (is_operator[c]) {
        token_read_operator(err, m, t);
	} else if ( c == EOF ) {
		t->type = EOF;
    } else {
        error_push(err, "unexpected character: %s (0x%02x)", PRINTABLE(c), c);
    }

    return t;
}

void token_print(Error* err, Token* t)
{
	int ok;
    char* start = t->start;
    ok = fprintf(stderr, "{\n\t\"");
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
    ok = fprintf(stderr, "\"\n\ttype: %s\n}\n", token_type_str[t->type]);
	if (ok < 0) {
		error_push(err, "failed to print token: %s", strerror(errno));
		return;
	}
}

