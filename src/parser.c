
#include "common.h"
#include "error.h"
#include "file_stream.h"
#include "printable.h"
#include "stack.h"
#include "tokenizer.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

/* ======= Grammar Rules =======

statement:
    : statements statement
    | statement;
statement
    : expr
    | assignment;
assignment
    : IDENTIFIER TYPE ASSIGNMENT expr

expr : <implemented without BNF or recursion, standard mathematics rules>

 ================================ */

typedef struct parser {
    Token* cur;
    Token* next;
    Mfile* m;
} TokenStream;

void parser_print_position(TokenStream* ts)
{
    Mfile m = *(ts->m);

    m.pos = 0;
    int linecount = 0;
    int col = 0;
    while (!mfile_eof(&m) && m.pos <= ts->m->pos) {
        char c = mfile_get(&m);
        if (c == '\n') {
            linecount++;
            col = 0;
        } else {
            col++;
        }
    }

    fprintf(stderr, "\nLine: %d\nCol: %d\n", linecount, col);
}

bool tokenstream_advance(Error* err, TokenStream* ts)
{
    ts->cur = ts->next;
    mfile_skip(ts->m, isspace);
    ts->next = token_read(err, ts->m);
    if (!error_empty(err)) {
        error_push(err, "%s failed", __func__);
        return false;
    }
    return true;
}

enum value_type {
    VALUE_INTEGER,
    VALUE_FLOATING,
};

static const char* value_type_str[] = {
    [VALUE_INTEGER]  = "VALUE_INTEGER",
    [VALUE_FLOATING] = "VALUE_FLOATING",
};

typedef struct value {
    const char* debug_name;
    enum value_type type;
    union {
        int64_t i64;
        double f64;
    };
} Value;

static void value_print(FILE* out, Value* v)
{
    switch (v->type) {
	case VALUE_INTEGER:
		fprintf(out, "%" PRId64, v->i64);
		break;
	case VALUE_FLOATING:
		fprintf(out, "%lf", v->f64);
		break;
	default:
		fprintf(out, "(bad value)");
		break;
    }
}

static Value* parse_int(Error* err, TokenStream* ts)
{
    Token* t = ts->cur;
    if (t->type != TOKEN_INTEGER) {
        error_push(err, "(%s) unexpected token type: %s", __func__, token_type_str[t->type]);
        return NULL;
    }
    Value* v = calloc(1, sizeof *v);
    if (!v) {
        error_push(err, "(%s) failed to allocate value: %s", __func__, strerror(errno));
        return NULL;
    }
    v->type = VALUE_INTEGER;
    assert(errno == 0);
    errno = 0;
    v->i64= strtol(t->start, NULL, 10);
    if (errno != 0) {
        error_push(err, "(%s) failed to parse int: %s", __func__, strerror(errno));
        return NULL;
    }
    tokenstream_advance(err, ts);
    return v;
}

static Value* parse_floating(Error* err, TokenStream* ts)
{
    Token* t = ts->cur;
    if (t->type != TOKEN_FLOATING) {
        error_push(err, "(%s) unexpected token type: %s", __func__, token_type_str[t->type]);
        return NULL;
    }
    Value* v = calloc(1, sizeof *v);
    if (!v) {
        error_push(err, "(%s) failed to allocate value: %s", __func__, strerror(errno));
        return NULL;
    }
    v->type = VALUE_FLOATING;
    assert(errno == 0);
    errno = 0;
    v->f64= strtod(t->start, NULL);
    if (errno != 0) {
        error_push(err, "(%s) failed to parse float: %s", __func__, strerror(errno));
        return NULL;
    }
    tokenstream_advance(err, ts);
    if (!error_empty(err)) {
        error_push(err, "(%s) couldn't advance parser", __func__);
        return NULL;
    }
    return v;
}

static void conv_int_to_float(Error* err, Value* val)
{
    if (val->type != VALUE_INTEGER) {
        error_push(err, "(%s) conversion from %s to float not implemented", __func__, value_type_str[val->type]);
        return;
    }
    fprintf(stderr, "converting %ld to %lf", val->i64, (double)val->i64);
    val->f64 = (double)val->i64; 
    val->type = VALUE_FLOATING;
}

static Value* binary_op(Error* err, Value* lval, Value* rval, Token* op)
{
    if ((lval->type != VALUE_INTEGER && lval->type != VALUE_FLOATING)
     || (rval->type != VALUE_INTEGER && rval->type != VALUE_FLOATING)
     || op->type    != TOKEN_OPERATOR)
    {
        error_push(err, "%s: unexpected token types: %s %s %s",
                __func__, value_type_str[lval->type],
                value_type_str[rval->type], token_type_str[op->type]);
        fprintf(stderr, "\n####\n");
        token_print(err, op);
        goto fail;
    }

    if (lval->type == VALUE_FLOATING && rval->type == VALUE_INTEGER) {
        conv_int_to_float(err, rval);
    } else if (lval->type == VALUE_INTEGER && rval->type == VALUE_FLOATING) {
        conv_int_to_float(err, lval);
    }
    if (!error_empty(err)) {
        error_push(err, "binary expression failed");
        goto fail;
    }

    Value* result = calloc(1, sizeof *result);
    if (!result) {
        error_push(err, "%s: failed to allocate value: %s", __func__, strerror(errno));
        goto fail;
    }

    //fprintf(stderr, "\ndoing op: %s %c %s", value_type_str[lval->type], op->start[0], value_type_str[rval->type]);
    if (rval->type == VALUE_INTEGER && lval->type == VALUE_INTEGER) {
        result->type = VALUE_INTEGER;
        switch (op->start[0]) {
        case '+':
            result->i64 = lval->i64 + rval->i64;
            break;
        case '*':
            result->i64 = lval->i64 * rval->i64;
            break;
        case '-':
            result->i64 = lval->i64 - rval->i64;
            break;
        case '/':
            result->i64 = lval->i64 / rval->i64;
            break;
        }
    } else if (rval->type == VALUE_FLOATING && lval->type == VALUE_FLOATING) {
        result->type = VALUE_FLOATING;
        switch (op->start[0]) {
        case '+':
            result->f64 = lval->f64 + rval->f64;
            break;
        case '*':
            result->f64 = lval->f64 * rval->f64;
            break;
        case '-':
            result->f64 = lval->f64 - rval->f64;
            break;
        case '/':
            result->f64 = lval->f64 / rval->f64;
            break;
        }
    }
	free(op);
    return result;

fail:
	return NULL;
}

static inline int8_t operator_precedence(Token* op)
{
    // wrapped in a function for now because operators longer than 1 char might
    // be implemented
    static const int8_t lookup[256] = {
        ['+'] = -20, ['-'] = -20,
        ['*'] = -10, ['/'] = -10,
        ['('] = 128, [')'] = 128
    };
    return lookup[(size_t)(op->start[0])];
}

static Value* parse_expr(Error* err, TokenStream* ts)
{
    FixedStack op_stack = STACK_INIT;
    FixedStack value_stack = STACK_INIT;

    fprintf(stderr, "PRATT START\n");

    while (1) {
        token_print(NULL, ts->cur);
        switch (ts->cur->type) {
        case TOKEN_INTEGER:
            stack_push(&value_stack, parse_int(err, ts));
            if (!error_empty(err))
                goto fail;
            break;

        case TOKEN_FLOATING:
            stack_push(&value_stack, parse_floating(err, ts));
            if (!error_empty(err))
                goto fail;
            break;

        case TOKEN_IDENTIFIER:
            fprintf(stderr, "identifiers not implemented yet\n");
            exit(FATAL_NOT_IMPLEMENTED);
            break;

        case TOKEN_PAREN_OPEN:
            stack_push(&op_stack, ts->cur);
            tokenstream_advance(err, ts);
            if (!error_empty(err))
                goto fail;
            break;

        case TOKEN_PAREN_CLOSE:
            while (!stack_empty(&op_stack) && ((Token*)stack_top(&op_stack))->type != TOKEN_PAREN_OPEN) {
                Value* rval = stack_pop(&value_stack);
                Value* lval = stack_pop(&value_stack);
                Token* op   = stack_pop(&op_stack);
                Value* result = binary_op(err, lval, rval, op);
                if (!error_empty(err))
                    goto fail;
                stack_push(&value_stack, result);
            }
            if (((Token*)stack_top(&op_stack))->type != TOKEN_PAREN_OPEN) {
                error_push(err, "%s: mismatched parentheses", __func__);
                return NULL;
            } else {
                stack_pop(&op_stack);
            }
            tokenstream_advance(err, ts);
            if (!error_empty(err))
                goto fail;

            break;

        case TOKEN_OPERATOR: {
            Token* new_op = ts->cur;
            if (!stack_empty(&op_stack)) {
                while (operator_precedence(new_op) < operator_precedence(stack_top(&op_stack))) {
                    Value* rval   = stack_pop(&value_stack);
                    Value* lval   = stack_pop(&value_stack);
                    Token* op     = stack_pop(&op_stack);
                    Value* result = binary_op(err, lval, rval, op);
                    if (!error_empty(err))
                        goto fail;
                    stack_push(&value_stack, result);
                }
            }
            tokenstream_advance(err, ts);
            if (!error_empty(err))
                goto fail;
            stack_push(&op_stack, new_op);
            break;}

        default:
            goto end;
        }
    }
end:
    while (!stack_empty(&value_stack) && !stack_empty(&op_stack)) {
        Token* op     = stack_pop(&op_stack);
        Value* rval   = stack_pop(&value_stack);
        Value* lval   = stack_pop(&value_stack);
        Value* result = binary_op(err, lval, rval, op);
        if (!error_empty(err))
            goto fail;
        stack_push(&value_stack, result);
    }
    if (stack_len(&value_stack) != 1 && stack_len(&op_stack) != 0) {
        error_push(err, "(%s) bad expression", __func__);
        goto fail;
    }
    fprintf(stderr, "PRATT END\n");
    return stack_top(&value_stack);
    
fail:
    return NULL;
}

static Value* parser_next(Error* err, TokenStream* ts)
{
    if (ts->cur->type == TOKEN_EOF || !error_empty(err)) {
        return NULL;
    }

    Value* result;
    switch (ts->cur->type) {
    case TOKEN_INTEGER:
    case TOKEN_FLOATING:
        result = parse_expr(err, ts);
        if (!error_empty(err) || result == NULL) {
            goto syntax_error;
        }
        fprintf(stderr, "result: ");
        value_print(stderr, result);
        fprintf(stderr, "\n");
        
        break;

    default: syntax_error:
        error_push(err, "(%s) syntax error: unexpected token %s (%s)", __func__,
                token_type_str[ts->cur->type], token_str(ts->cur));
        return NULL;
    }

    if (ts->cur->type != TOKEN_STATEMENT_END) {
        error_push(err, "(%s) expected semicolon", __func__);
        return NULL;
    }
    tokenstream_advance(err, ts);

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

    Error err = ERROR_INIT;
    Mfile* m = mfile_open(&err, argv[1]);
    if (!error_empty(&err)) {
        error_push(&err, "mfile_open");
        error_print(&err);
        return EXIT_FAILURE;
    }

    TokenStream ts = {
        .cur = NULL,
        .next = NULL,
        .m = m,
    };
    tokenstream_advance(&err, &ts);
    tokenstream_advance(&err, &ts);

    while (!mfile_eof(m)) {
        parser_next(&err, &ts);
        if (!error_empty(&err)) {
            error_print(&err);
            parser_print_position(&ts);
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
