
#include "error.h"
#include "file_stream.h"
#include "tokenizer.h"
#include "printable.h"

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

/* ======= Grammar Rules =======

statement:
    : statements statement
    | statement;
statement
    | expr
expr
    : INT OPERATOR INT     {$$ = binary_op_int($1, $3, $2);}
    | INT OPERATOR FLOAT   {conv($1); $$ = binary_op_float($1, $3, $2);}
    | FLOAT OPERATOR INT   {conv($2); $$ = binary_op_float($1, $3, $2);}
    | FLOAT OPERATOR FLOAT {$$ = binary_op_float($1, $3, $2);}
    ;
   ============================= */

enum value_type {
	VALUE_INTEGER,
	VALUE_OPERATOR,
	VALUE_TYPE_COUNT,
};

static const char* value_type_str[VALUE_TYPE_COUNT] = {
	[VALUE_INTEGER] = "VALUE_INTEGER",
	[VALUE_OPERATOR] = "VALUE_OPERATOR",
};

typedef struct value {
	const char* debug_name;
	enum value_type type;
	union {
		int64_t i;
		char op[3];
	};
} Value;

typedef struct symbol_table_entry {
    const char* debug_name;
	int id;
    Value* val;
    struct symbol_table_entry* next;
} Symbol_table_entry;

typedef struct symbol_table {
    Symbol_table_entry* syms;
} Symbol_table;

static Value* parse_operator(Error* err, Token* t)
{
	if (t->type != TOKEN_OPERATOR) {
		error_push(err, "%s: unexpected token type: %s", __func__, token_type_str[t->type]);
		return NULL;
	}
	Value* v = malloc(sizeof *v);
	if (!v) {
		error_push(err, "%s: failed to allocate value: %s", __func__, strerror(errno));
		return NULL;
	}
	v->type = VALUE_OPERATOR;
	strncpy(v->op, t->start, MIN(t->end - t->start, sizeof(v->op)));
	return v;
}

static Value* parse_int(Error* err, Token* t)
{
	if (t->type != TOKEN_INTEGER) {
		error_push(err, "%s: unexpected token type: %s", __func__, token_type_str[t->type]);
		return NULL;
	}
	Value* v = malloc(sizeof *v);
	if (!v) {
		error_push(err, "%s: failed to allocate value: %s", __func__, strerror(errno));
		return NULL;
	}
	v->type = VALUE_INTEGER;
	assert(errno == 0);
	errno = 0;
	v->i = strtol(t->start, NULL, 10);
	if (errno != 0) {
		error_push(err, "%s: failed to parse int: %s", __func__, strerror(errno));
		return NULL;
	}
	return v;
}

static Value* parse_binary_expr(Error* err, Value* lval, Value* rval, Value* op)
{
	if (lval->type != VALUE_INTEGER
	 || rval->type != VALUE_INTEGER
	 || op->type   != VALUE_OPERATOR)
	{
		error_push(err, "%s: unexpected token types: %s %s %s",
				__func__, value_type_str[lval->type],
				value_type_str[rval->type], value_type_str[op->type]);
		return NULL;
	}

	Value* result = malloc(sizeof *result);
	if (!result) {
		error_push(err, "%s: failed to allocate value: %s", __func__, strerror(errno));
		return NULL;
	}
	result->type = VALUE_INTEGER;
	switch (op->op[0]) {
	case '+':
		result->i = lval->i + rval->i;
		break;
	case '*':
		result->i = lval->i * rval->i;
		break;
	case '-':
		result->i = lval->i - rval->i;
		break;
	case '/':
		result->i = lval->i / rval->i;
		break;
	}
	printf("\nCALCULATED EXPRESSION %ld %c %ld = %ld\n", lval->i, op->op[0], rval->i, result->i);
	return result;
}

static Value* parser_handle_binary_expr(Error* err, Token* peek, Mfile* m)
{
		Value* lval = parse_int(err, peek);
		if (!error_empty(err) || !lval) {
			goto generic_error;
		}

		Token* op_tok = token_read(err, m);
		if (!error_empty(err)) {
			goto generic_error;
		} else if (op_tok->type != TOKEN_OPERATOR) {
			goto syntax_error;
		}

		Value* op = parse_operator(err, op_tok);
		if (!error_empty(err)) { 
			goto generic_error;
		}

		Token* rval_tok = token_read(err, m);
		if (!error_empty(err)) {
			goto generic_error;
		} else if (rval_tok->type != TOKEN_INTEGER) {
			goto syntax_error;
		}
		Value* rval = parse_int(err, rval_tok);
		if (!error_empty(err)) {
			goto generic_error;
		}

		Value* result = parse_binary_expr(err, lval, rval, op);
		if (!error_empty(err)) {
			goto generic_error;
		}
		return result;

syntax_error:
		error_push(err, "%s: syntax error, expected binary expression", __func__);
generic_error:
		return NULL;
}

static Value* parser_next(Error* err, Mfile* m)
{
	mfile_skip(m, isspace);
	Token* t = token_read(err, m);
	if (t->type == EOF) {
		return NULL;
	} else if (!error_empty(err)) {
		return NULL;
	}

	Value* result;

	switch (t->type) {
		case TOKEN_INTEGER: {
			result = parser_handle_binary_expr(err, t, m);
			if (!error_empty(err)) {
				goto syntax_error;
			}
		}
		default: syntax_error: {
			error_push(err, "syntax error: unexpected token %s", token_type_str[t->type]);
			return NULL;
		}
	}
	token_print(err, t);
	return result;
}

/* ========================================================================= */

int main(int argc, char** argv)
{
    int status = EXIT_SUCCESS;

	if (argc != 2) {
		fprintf(stderr, "usage: %s <file>\n", argv[0]);
		return EXIT_FAILURE;
	}

	// create a protected page for debugging purposes
	size_t pagesize = sysconf(_SC_PAGESIZE);
    void* protected_page = mmap(NULL, pagesize, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
	mfile_overflow_slope = protected_page + 0;
    mprotect(protected_page, pagesize, PROT_NONE);

    Error err = ERROR_INIT;
    Mfile* m = mfile_open(&err, argv[1]);
    if (!error_empty(&err)) {
        error_push(&err, "mfile_open");
        error_print(&err);
        return EXIT_FAILURE;
    }

	while (!mfile_eof(m)) {
		parser_next(&err, m);
		if (!error_empty(&err)) {
			error_print(&err);
			return EXIT_FAILURE;
		}
	}


    mfile_close(&err, m);
    if (!error_empty(&err)) {
        error_push(&err, "mfile_close");
        error_print(&err);
        return EXIT_FAILURE;
    }

    return status;
}
