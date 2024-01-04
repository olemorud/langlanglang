
#include "file_stream.h"
#include <string.h>
#include <stdio.h>

int main(int argc, char** argv)
{
    int status = EXIT_SUCCESS;

    fprintf(stderr, "attempting to open test.txt\n");
    Error err = ERROR_INIT;
    Mfile* m = mfile_open(&err, "test.txt");
    if (!error_empty(&err)) {
        error_push(&err, "mfile_open test failed");
        error_print(&err);
        status = EXIT_FAILURE;
    } else {
        fprintf(stderr, "OK\n");
    }

    fprintf(stderr, "attempting to read from test.txt with mfile_get(m)\n");
    char str[12];
    size_t n = 0;
    while (!mfile_eof(m) && n < 12) {
        str[n++] = mfile_get(m);
    }
    if (strncmp("the brown fox", str, n) != 0) {
        error_push(&err, "mfile_get test failed");
        error_print(&err);
        status = EXIT_FAILURE;
    } else {
        fprintf(stderr, "OK\n");
    }

    fprintf(stderr, "attempting to close\n");
    mfile_close(&err, m);
    if (!error_empty(&err)) {
        error_push(&err, "mfile_close test failed");
        error_print(&err);
        status = EXIT_FAILURE;
    } else {
        fprintf(stderr, "OK\n");
    }

    return status;
}
