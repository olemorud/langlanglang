
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
    }
    close(s->fd);
    if (ok == -1) {
        error_push(err, "failed to close file: %s", strerror(errno));
    }
    free(s);
}

inline size_t mfile_inc_pos(Mfile* m)
{
    return m->pos++;
}
inline size_t mfile_decr_pos(Mfile* m)
{
    return m->pos--;
}

inline int mfile_get(Mfile* m)
{
    if (m->pos >= m->size) {
        return EOF;
    }
    return m->data[mfile_inc_pos(m)];
}

inline bool mfile_eof(Mfile* m)
{
    return m->pos >= m->size;
}

inline char* mfile_cur(Mfile* m)
{
    static char eof = EOF;
    if (m->pos >= m->size) {
        return &eof;
    }
    return m->data + m->pos;
}

inline int mfile_curchar(Mfile* m)
{
    return *(mfile_cur(m));
}

void mfile_skip(Mfile* m, int (*f)(int))
{
    while (f(mfile_curchar(m)))
        mfile_inc_pos(m);
}




