#pragma once

#include <sys/stat.h>
#include <stdlib.h>
#include "error.h"

extern char* mfile_overflow_slope;

typedef struct mfile {
	char* data;
	size_t size;
	size_t pos;

    // internal
    int fd;
    struct stat sb;
    #define mfile_size sb.st_size
} Mfile;

/* Open memory mapped file */
Mfile* mfile_open(Error* err, char* filename);

/* Close memory mapped file */
void mfile_close(Error* err, Mfile* s);

/* Get next byte, returns EOF if end of file */
int mfile_get(Mfile* s);

/* Returns true if end of file */
bool mfile_eof(Mfile* s);

/* Returns current position */
char* mfile_cur(Mfile* s);

/* Skips char until f is false */
void mfile_skip(Mfile* s, int (*f)(int));

/* Get current char */
int mfile_curchar(Mfile* s);

/* Skip the current char */
size_t mfile_inc_pos(Mfile* m);

size_t mfile_decr_pos(Mfile* m);
