
#include <stdlib.h>
#include "error.h"
#include "file_stream.h"

int main(int argc, char** argv)
{
    int status = EXIT_SUCCESS;

    Error err = ERROR_INIT;
    Mfile* m = mfile_open(&err, "test.txt");
    if (!error_empty(&err)) {
        error_push(&err, "mfile_open");
        error_print(&err);
        status = EXIT_FAILURE;
    }

    mfile_close(&err, m);
    if (!error_empty(&err)) {
        error_push(&err, "mfile_close");
        error_print(&err);
        status = EXIT_FAILURE;
    }


    return status;
}
