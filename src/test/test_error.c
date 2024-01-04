
#include "error.h"
#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

int bad_alloc(struct error* err)
{
    void* bad = malloc(9223372036854775807UL);
    if (!bad) {
        error_push(err, "bad_alloc failed: malloc: %s", strerror(errno));
        return -1;
    }
    return 0;
}

int failer(struct error* err)
{
    bad_alloc(err);
    if (!error_empty(err)) {
        error_push(err, "failer failed");
        return -1;
    }
    return 0;
}

int winner(struct error* err)
{
    return 1;
}

int main()
{
    int status = EXIT_SUCCESS;

    fprintf(stderr, "Running function that results in error\n");
    struct error err = ERROR_INIT;
    failer(&err);
    if (!error_empty(&err)) {
        fprintf(stderr, "OK\n");
    } else {
        fprintf(stderr, "function `failer` did not fail\n");
        status = EXIT_FAILURE;
    }

    error_clear(&err);

    fprintf(stderr, "Running function that should not return error\n");
    winner(&err);
    if (!error_empty(&err)) {
        error_print(&err);
        fprintf(stderr, "function `winner` failed when it's not supposed to");
        status = EXIT_FAILURE;
    } else {
        fprintf(stderr, "OK\n");
    }

    error_clear(&err);

    return status;
}
