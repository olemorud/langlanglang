
#pragma once

#include <stddef.h>
#include <stdbool.h>

struct error_msg {
    struct error_msg* next;
    char* message;
};

typedef struct error {
    struct error_msg* msg;
} Error;

/* Returns true if there is no error */
bool error_empty(Error* err);

/* Add message to error */
void error_push_(Error* err, const char* fmt, ...);
#define error_push(err, fmt, args...) error_push_(err, "(%s) " fmt, __func__ __VA_OPT__(,) args)

/* Print error */
void error_print(Error* err);

/* Frees the internal data structures but leaves err in a usable state  */
void error_clear(Error* err);

#define ERROR_INIT {.msg = NULL}
