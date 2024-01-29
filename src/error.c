
#include "error.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool error_empty(Error* err)
{
    if (!err)
        return false;
    return err->msg == NULL;
}

void error_push(Error* err, const char* fmt, ...)
{
	if (!err)
		return;

	#define MSG_SIZE 128

    // first list element is a dummy element
    if (err->msg == NULL) {
        err->msg = malloc(sizeof *err->msg);
        if (!(err->msg)) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }
    }

    struct error_msg* m = err->msg;

	while (m->next) {
		m = m->next;
	}
	m->next = malloc(sizeof *m);
	if (m->next == NULL) {
		perror("malloc");
        exit(EXIT_FAILURE);
	}
	m->next->message = malloc(MSG_SIZE);
	if (!(m->next->message)) {
		perror("malloc");
        exit(EXIT_FAILURE);
	}
	va_list args;
	va_start(args, fmt);
	vsnprintf(m->next->message, MSG_SIZE, fmt, args);
	va_end(args);

	#undef MSG_SIZE
}

static void error_msg_print(struct error_msg* msg, bool print_colon)
{
	if (!msg)
		return;
    error_msg_print(msg->next, true);

	if (msg->message) {
		fprintf(stderr, "%s", msg->message);
	}

    if (print_colon) {
        fprintf(stderr, "\n - ");
    }
}

void error_print(Error* err)
{
    if (!err) {
        fprintf(stderr, "(empty error)\n");
        return;
    }
    error_msg_print(err->msg, false);
    fprintf(stderr, "\n");
}

static void error_msg_free(struct error_msg* msg)
{
    if (!msg)
        return;
    error_msg_free(msg->next);
    free(msg->message);
    msg->message = NULL;
    msg->next    = NULL;
    free(msg);
}

void error_clear(Error* err)
{
    if (!err)
        return;

    error_msg_free(err->msg);
    err->msg = NULL;
}

