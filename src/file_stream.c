
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "error.h"
#include "file_stream.h"

char* mfile_overflow_slope = NULL;

Mfile* mfile_open(Error* err, char* filename)
{
	Mfile* s = malloc(sizeof *s);
	if (!s) {
		error_push(err, "failed to allocate file stream struct: %s", strerror(errno));
        goto malloc_fail;
	}

	s->fd = open(filename, O_RDONLY);
	if (s->fd == -1) {
		error_push(err, "failed to open file %s: %s", filename, strerror(errno));
        goto open_fail;
	}

	struct stat sb;
	int ok = fstat(s->fd, &sb);
	if (ok == -1) {
		error_push(err, "failed to stat file %s: %s", filename, strerror(errno));
        goto stat_fail;
	}
	s->size = sb.st_size;

	s->data = mmap(NULL, s->size, PROT_READ, MAP_PRIVATE, s->fd, 0);
	if (s->data == MAP_FAILED) {
		error_push(err, "failed to mmap file: %s", strerror(errno));
        goto mmap_fail;
	}

    return s;

mmap_fail:
stat_fail:
    close(s->fd);
open_fail:
    free(s);
malloc_fail:
    return NULL;
}

void mfile_close(Error* err, Mfile* s)
{
    int ok;
    ok = munmap(s->data, s->size);
    if (ok == -1) {
        error_push(err, "failed to munmap file: %s", strerror(errno));
        free(s);
        return;
    }
    close(s->fd);
    if (ok == -1) {
        error_push(err, "failed to close file: %s", strerror(errno));
        free(s);
        return;
    }
    free(s);
}

int mfile_get(Mfile* s)
{
    if (s->pos >= s->size) {
        return EOF;
    }
    return s->data[s->pos++];
}

bool mfile_eof(Mfile* s)
{
    return s->pos >= s->size;
}

char* mfile_cur(Mfile* s)
{
    if (s->pos >= s->size) {
        return mfile_overflow_slope;
    }
    return s->data + s->pos;
}

int mfile_curchar(Mfile* s)
{
    if (s->pos >= s->size) {
        return EOF;
    }
    return *(s->data + s->pos);
}

void mfile_skip(Mfile* s, int (*f)(int))
{
    while (f(*(s->data + s->pos)))
        s->pos += 1;
}




