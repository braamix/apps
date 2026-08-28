/*	FILEIO.C
 *
 * The routines in this file read and write ASCII files from the disk. All of
 * the knowledge about files are here.
 *
 *	modified by Petri Kutvonen
 */

#include "braam.h"
#include "estruct.h"
#include "globals.h"
#include "efunc.h"
#include "builtin.h"

#include "proc/io.h"

/*
 * There is no stdio, so the FILE * is a descriptor and a buffer of our own.
 * Reading refills from read_some(); writing accumulates and goes out at
 * file_close(), which is where a partial write can still be reported.
 */
#define FIOBUF 4096

static int ffd = -1; /* the open file, -1 for none */
static int eofflag;  /* end-of-file flag */
static int ferr;     /* a read or write failed */

static char *fbuf; /* the buffer, FIOBUF bytes */
static int fhave;  /* bytes in it: read or unwritten */
static int fnext;  /* how far read, reading only */

static int fwriting; /* which of the two a file is open for */

/* The compiled-in file being read, and how far, when ffd is -1. */
static const struct builtin_file *fbuiltin;
static unsigned fbpos;

/*
 * The compiled-in file of that name, or NULL.
 *
 * A name with a directory in it never matches, and that is the whole of the
 * ordering: lookup_file() tries $HOME/<name>, then $HOME/lib/<name>, then the
 * bare name in the current directory, and only the last of those can be a
 * built-in.  Matching the leaf of every spelling would answer the first probe
 * instead, and a copy on disk anywhere but $HOME would never be reached.
 */
static const struct builtin_file *builtin_of(const char *fn)
{
    const struct builtin_file *b;

    if (strchr(fn, '/'))
        return NULL;
    for (b = builtin_files; b->name; b++)
        if (strcmp(fn, b->name) == 0)
            return b;
    return NULL;
}

static int getbuf(void)
{
    if (!fbuf)
        fbuf = (char *)malloc(FIOBUF);
    return fbuf != NULL;
}

/*
 * Open a file for reading.
 */
Task<int> file_open_read(char *fn)
{
    Result<i32> r = Err(Error::NoMemory);

    if (!getbuf())
        co_return FIOMEM;
    if (Task<Result<i32>> t = open_read(Str(fn, strlen(fn))))
        r = co_await t;
    eofflag = FALSE;
    ferr    = FALSE;
    fhave = fnext = 0;
    fwriting      = FALSE;
    fbuiltin      = NULL;
    fbpos         = 0;

    /* Nothing on disk: .emacsrc and emacs.hlp are compiled in. */
    if (r.is_err()) {
        ffd      = -1;
        fbuiltin = builtin_of(fn);
        co_return fbuiltin ? FIOSUC : FIOFNF;
    }
    ffd = r.value();
    co_return FIOSUC;
}

/*
 * Open a file for writing. Return TRUE if all is well, and FALSE on error
 * (cannot create).
 */
Task<int> file_open_write(char *fn)
{
    Result<i32> r = Err(Error::NoMemory);

    if (!getbuf()) {
        msg_printf("Cannot open file for writing");
        co_return FIOERR;
    }
    if (Task<Result<i32>> t =
            open_at(Str(fn, strlen(fn)), SYS_O_WRITE | SYS_O_CREATE | SYS_O_TRUNC))
        r = co_await t;
    if (r.is_err()) {
        msg_printf("Cannot open file for writing");
        co_return FIOERR;
    }
    ffd   = r.value();
    ferr  = FALSE;
    fhave = fnext = 0;
    fwriting      = TRUE;
    fbuiltin      = NULL;
    co_return FIOSUC;
}

/* What has been put but not written. */
static Task<int> fdrain(void)
{
    Result<void> r = Err(Error::NoMemory);

    if (fhave == 0)
        co_return TRUE;
    if (Task<Result<void>> t = write_all((u32)ffd, Str(fbuf, (usize)fhave)))
        r = co_await t;
    fhave = 0;
    if (r.is_err()) {
        ferr = TRUE;
        co_return FALSE;
    }
    co_return TRUE;
}

/*
 * Close a file. Should look at the status in all systems.
 */
Task<int> file_close(void)
{
    int ok = TRUE;

    /* free this since we do not need it anymore */
    if (file_line) {
        free(file_line);
        file_line = NULL;
    }
    eofflag = FALSE;

    if (fwriting)
        ok = co_await fdrain();
    if (ffd >= 0) {
        if (Task<void> t = close_fd((u32)ffd))
            co_await t;
        ffd = -1;
    }
    fwriting = FALSE;
    fbuiltin = NULL;
    if (!ok || ferr) {
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
    int i;

    for (i = 0; i <= nbuf; ++i) {
        if (fhave == FIOBUF && !co_await fdrain())
            break;
        fbuf[fhave++] = i < nbuf ? (buf[i] & 0xFF) : '\n';
    }

    if (ferr) {
        msg_printf("Write I/O error");
        co_return FIOERR;
    }

    co_return FIOSUC;
}

/*
 * One byte out of the buffer, or -1 when it is dry.  A plain function on
 * purpose: a co_await is a call and not a tail call here, so awaiting once
 * per byte would grow the native stack by the length of the file.  frefill()
 * is the awaited half, and it suspends once per FIOBUF bytes, which is what
 * gives the stack back.
 */
static int fgetbyte(void)
{
    if (fnext < fhave)
        return (unsigned char)fbuf[fnext++];
    if (fbuiltin) {
        if (fbpos >= fbuiltin->size)
            return -1;
        return (unsigned char)fbuiltin->text[fbpos++];
    }
    return -1;
}

/* More bytes, or FALSE at the end of the file. */
static Task<int> frefill(void)
{
    Result<String> r = Err(Error::NoMemory);

    if (fbuiltin || eofflag || ferr || ffd < 0)
        co_return FALSE;

    if (Task<Result<String>> t = read_some((u32)ffd, FIOBUF))
        r = co_await t;
    if (r.is_err()) {
        if (r.error() != Error::Closed)
            ferr = TRUE;
        eofflag = TRUE;
        co_return FALSE;
    }
    {
        Str got = r.value().str();

        fhave = (int)(got.size() < FIOBUF ? got.size() : FIOBUF);
        fnext = 0;
        if (fhave == 0) {
            eofflag = TRUE;
            co_return FALSE;
        }
        memcpy(fbuf, got.data(), (usize)fhave);
    }
    co_return TRUE;
}

/*
 * Read a line from a file, and store the bytes in the supplied buffer. The
 * "nbuf" is the length of the buffer. Complain about long lines and lines
 * at the end of the file that don't have a newline present. Check for I/O
 * errors too. Return status.
 *
 * Upstream had two paths, fgets() and a byte loop, and took the second only
 * for -n; there is no fgets here, so the byte loop is the only one and -n is
 * what decides whether a NUL is kept.
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
    c = fgetbyte();
    if (c < 0 && co_await frefill())
        c = fgetbyte();
    while (c >= 0 && c != '\n') {
        /*
         * A NUL without -n ended the line: fgets() stopped at it and
         * ate the rest, so this does too.  With -n it is dropped and
         * the line goes on, which is what the byte loop always did.
         */
        if (c == 0 && !accept_nulls) {
            for (;;) {
                c = fgetbyte();
                if (c < 0 && co_await frefill())
                    continue;
                if (c < 0 || c == '\n')
                    break;
            }
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
        c = fgetbyte();
        if (c < 0 && co_await frefill())
            c = fgetbyte();
    }

    /* test for any errors that may have occured */
    if (c < 0) {
        if (ferr) {
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
    Result<FileInfo> r = Err(Error::NoMemory);

    if (Task<Result<FileInfo>> t = stat_of(Str(fname, strlen(fname))))
        r = co_await t;
    if (!r.is_err())
        co_return TRUE;
    co_return builtin_of(fname) ? TRUE : FALSE;
}
