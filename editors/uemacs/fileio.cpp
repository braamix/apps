/*	FILEIO.C
 *
 * The routines in this file read and write ASCII files from the disk. All of
 * the knowledge about files are here.
 *
 *	modified by Petri Kutvonen
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "estruct.h"
#include "globals.h"
#include "efunc.h"

#include "compat/cio.h"

/*
 * One stream at a time, as upstream had one FILE *.  The kit's is stdio's
 * shape -- a buffer, a sticky error, a flush -- so the descriptor, the
 * buffer, the two counts and the refill this file used to carry are gone.
 */
static FILE *ffp;

/*
 * Open a file for reading.
 */
Task<int> file_open_read(char *fn)
{
    ffp = co_await b_fopen(fn, "r");
    co_return ffp == NULL ? FIOFNF : FIOSUC;
}

/*
 * Open a file for writing. Return TRUE if all is well, and FALSE on error
 * (cannot create).
 */
Task<int> file_open_write(char *fn)
{
    ffp = co_await b_fopen(fn, "w");
    if (ffp == NULL) {
        msg_printf("Cannot open file for writing");
        co_return FIOERR;
    }
    co_return FIOSUC;
}

/*
 * Close a file. Should look at the status in all systems.
 */
Task<int> file_close(void)
{
    int bad;

    /* free this since we do not need it anymore */
    if (file_line) {
        free(file_line);
        file_line = NULL;
    }

    if (ffp == NULL)
        co_return FIOSUC;
    /* fclose flushes, so a write held back until now is reported here. */
    bad = co_await b_fclose(ffp);
    ffp = NULL;
    if (bad) {
        msg_printf("Error closing file");
        co_return FIOERR;
    }
    co_return FIOSUC;
}

/*
 * Write a line to the already opened file. The "buf" points to the buffer,
 * and the "nbuf" is its length, less the free newline. Return the status.
 * Check only at the newline.
 */
Task<int> file_put_line(char *buf, int nbuf)
{
    if (nbuf > 0 && co_await b_fwrite(buf, 1, (size_t)nbuf, ffp) != (size_t)nbuf)
        goto bad;
    if (co_await b_fputc('\n', ffp) == EOF)
        goto bad;
    co_return FIOSUC;

bad:
    msg_printf("Write I/O error");
    co_return FIOERR;
}

/*
 * Read a line from a file, and store the bytes in the supplied buffer. The
 * "nbuf" is the length of the buffer. Complain about long lines and lines
 * at the end of the file that don't have a newline present. Check for I/O
 * errors too. Return status.
 *
 * Upstream had two paths, fgets() and a byte loop, and took the second only
 * for -n; the byte loop is the only one here and -n is what decides whether a
 * NUL is kept.  b_fgetc is an awaiter and not a Task, so a byte costs no
 * coroutine frame -- which is the whole reason this may be a plain loop.
 */
Task<int> file_get_line(void)
{
    int c;         /* current character read */
    int i;         /* current index into fline */
    char *tmpline; /* temp storage for expanding line */

    /* dump fline if it ended up too big */
    if (file_line_size > NSTRING) {
        free(file_line);
        file_line = NULL;
    }

    /* if we don't have an fline, allocate one */
    if (file_line == NULL)
        if ((file_line = (char *)malloc(file_line_size = NSTRING)) == NULL)
            co_return FIOMEM;

    /* read the line in */
    i = 0;
    while ((c = co_await b_fgetc(ffp)) >= 0 && c != '\n') {
        /*
         * A NUL without -n ended the line: fgets() stopped at it and
         * ate the rest, so this does too.  With -n it is dropped and
         * the line goes on, which is what the byte loop always did.
         */
        if (c == 0 && !accept_nulls) {
            while ((c = co_await b_fgetc(ffp)) >= 0 && c != '\n')
                ;
            break;
        }
        if (c) {
            file_line[i++] = c;
            /* if it's longer, get more room */
            if (i >= file_line_size) {
                if ((tmpline = (char *)malloc(file_line_size + NSTRING)) == NULL)
                    co_return FIOMEM;
                memcpy(tmpline, file_line, (usize)file_line_size);
                file_line_size += NSTRING;
                free(file_line);
                file_line = tmpline;
            }
        }
    }

    /* test for any errors that may have occured */
    if (c < 0) {
        if (b_ferror(ffp)) {
            msg_printf("File read error");
            co_return FIOERR;
        }

        if (i == 0)
            co_return FIOEOF;
    }

    /* terminate the string */
    file_line[i] = 0;
    co_return FIOSUC;
}

/*
 * does <fname> exist on disk?
 *
 * char *fname;		file to check for existance
 */
Task<int> file_exists(char *fname)
{
    co_return co_await b_access(fname, F_OK) == 0 ? TRUE : FALSE;
}
