// zip.cpp — zip.c, by Mark Adler. The messages, and the driver.
//
// The option table and its dispatch — eighty-odd options and the nine hundred
// lines of fileio.c that parse them — arrive with the whole create path. What
// is here is upstream's message routines, which every other file calls, and
// enough of zipmain() to write an archive from the files named on the command
// line: the order it does things in is upstream's, and it grows into the rest.

#include "zip.h"

#include "crc32.h"
#include "proc/rt.h"
#include "revision.h"

local ZCONST char *USAGE = "usage: zip [-0..-9] [-q] <archive> <file>...\n";

// ------------------------------------------------------------------ messages

// Upstream ended the process here. Sys::Exit only records a status and a
// process ends when its root task returns, so the code is kept and the caller
// unwinds with it; zip_fatal_h is what a plain function leaves behind when it
// cannot write a message at all.
void zip_fail(int c, ZCONST char *h)
{
    if (zip_fatal == ZE_OK) {
        zip_fatal   = c;
        zip_fatal_h = h;
    }
}

Task<void> ziperr_msg(int c, ZCONST char *h)
{
    zip_fail(c, h);

    if (mesg_line_started) {
        co_await zfputc('\n', mesg);
        mesg_line_started = 0;
    }
    co_await zfprintf(zstderr, "\nzip error: %s (%s)\n", ZIPERRORS(c), h ? h : "");
    co_await zfflush(zstderr);
}

Task<void> zipwarn(ZCONST char *a, ZCONST char *b)
{
    if (mesg_line_started) {
        co_await zfputc('\n', mesg);
        mesg_line_started = 0;
    }
    co_await zfprintf(zstderr, "\tzip warning: %s%s\n", a, b);
    co_await zfflush(zstderr);
}

Task<void> zipmessage_nl(ZCONST char *a, int nl)
{
    if (noisy) {
        co_await zfprintf(mesg, "%s", a);
        if (nl) {
            co_await zfputc('\n', mesg);
            mesg_line_started = 0;
        } else {
            mesg_line_started = 1;
        }
        co_await zfflush(mesg);
    }
}

Task<void> zipmessage(ZCONST char *a, ZCONST char *b)
{
    if (noisy) {
        if (mesg_line_started) {
            co_await zfputc('\n', mesg);
            mesg_line_started = 0;
        }
        co_await zfprintf(mesg, "%s%s\n", a, b);
        co_await zfflush(mesg);
    }
}

// ------------------------------------------------------------------ the list

// Add one name to zfiles, as newname() will once the whole create path is
// here. The entry is filled in by zipup().
local Task<int> add_entry(char *name)
{
    struct zlist far *z = (struct zlist far *)malloc(sizeof(struct zlist));
    if (z == NULL)
        ZIPERR(ZE_MEM, "adding files")
    memset((char *)z, 0, sizeof(struct zlist));

    int dosflag = 0;
    z->name     = in2ex(name);
    z->iname    = ex2in(name, 0, &dosflag);
    if (z->name == NULL || z->iname == NULL)
        ZIPERR(ZE_MEM, "adding files")
    z->zname = in2ex(z->iname);
    z->oname = in2ex(z->iname);
    z->uname = in2ex(z->iname);
    if (z->zname == NULL || z->oname == NULL || z->uname == NULL)
        ZIPERR(ZE_MEM, "adding files")
    z->dosflag = dosflag;
    z->mark    = 1;

    // In order, so the archive lists in the order the names were given.
    struct zlist far **p = &zfiles;
    while (*p != NULL)
        p = &(*p)->nxt;
    *p = z;
    zcount++;
    co_return ZE_OK;
}

// ------------------------------------------------------------------- the run

local Task<int> zipmain(Args args)
{
    int r;

    mesg    = zstdout;
    zstderr = &File::stderr();

    init_upper();
    crc_32_tab = get_crc_table();
    co_await zclock_init();

    // The options the driver understands until the table arrives.
    usize i = 0;
    for (; i < args.size(); i++) {
        Str a = args[i];
        if (a.size() < 2 || a[0] != '-')
            break;
        if (a == "--") {
            i++;
            break;
        }
        for (usize j = 1; j < a.size(); j++) {
            char c = a[j];
            if (c >= '0' && c <= '9') {
                level  = c - '0';
                method = level == 0 ? STORE : BEST;
            } else if (c == 'q') {
                noisy = 0;
            } else if (c == 'v') {
                verbose = 1;
            } else {
                co_await zfprintf(zstderr, "zip: invalid option -- %c\n", c);
                co_await zfputs(USAGE, zstderr);
                co_return ZE_PARMS;
            }
        }
    }

    if (args.size() - i < 2) {
        co_await zfputs(USAGE, zstderr);
        co_return ZE_PARMS;
    }

    // The archive, and then the names.
    Str name = args[i++];
    zipfile  = (char *)malloc(name.size() + 1);
    if (zipfile == NULL)
        ZIPERR(ZE_MEM, "was processing arguments")
    memcpy(zipfile, name.data(), name.size());
    zipfile[name.size()] = 0;
    out_path             = zipfile;

    for (; i < args.size(); i++) {
        char *p = (char *)malloc(args[i].size() + 1);
        if (p == NULL)
            ZIPERR(ZE_MEM, "was processing arguments")
        memcpy(p, args[i].data(), args[i].size());
        p[args[i].size()] = 0;
        if ((r = co_await add_entry(p)) != ZE_OK) {
            free(p);
            co_return r;
        }
        free(p);
    }

    // Open the archive. SYS_O_EXCL beside SYS_O_CREATE is what mkstemp gave
    // upstream: a name nobody else has taken.
    tempzip = tempname(zipfile);
    if (tempzip == NULL)
        ZIPERR(ZE_MEM, "allocating temp filename")

    Result<i32> fd = Err(Error::NoMemory);
    if (Task<Result<i32>> t =
            open_at(Str(tempzip, strlen(tempzip)),
                    SYS_O_READ | SYS_O_WRITE | SYS_O_CREATE | SYS_O_EXCL | SYS_O_TRUNC))
        fd = co_await t;
    if (fd.is_err())
        ZIPERR(ZE_TEMP, tempzip)

    // File::of() borrows the descriptor and will not close it, and the store
    // refuses to rename a file something still holds open — so the descriptor
    // is closed by hand once the stream has been flushed.
    u32 yfd  = (u32)fd.value();
    FILE tmp = File::of(yfd, FileMode::Update);
    tmp.set_buffering(Buffering::Full);
    tmp.reserve(SYS_READ_MAX);
    y               = &tmp;
    output_seekable = 1;

    // Write the entries.
    tempzn = 0;
    for (struct zlist far *z = zfiles; z != NULL; z = z->nxt) {
        if (noisy) {
            co_await zfprintf(mesg, "  adding: %s", z->oname);
            mesg_line_started = 1;
            co_await zfflush(mesg);
        }
        r = co_await zipup(z);
        if (r != ZE_OK && r != ZE_OPEN && r != ZE_MISS)
            ZIPERR(r, z->oname)
        if (r == ZE_OPEN || r == ZE_MISS) {
            z->mark = 0;
            if (noisy) {
                co_await zfputc('\n', mesg);
                mesg_line_started = 0;
            }
            co_await zipwarn(r == ZE_OPEN ? "could not open for reading: " : "name not matched: ",
                             z->oname);
        }
        if (zip_fatal != ZE_OK)
            co_return zip_fatal;
    }

    // Write central directory and end header.
    extent k  = 0;      // keep count for end header
    uzoff_t c = tempzn; // get start of central
    uzoff_t n = 0, t = 0;
    for (struct zlist far *z = zfiles; z != NULL; z = z->nxt) {
        if (!z->mark)
            continue;
        if ((r = co_await putcentral(z)) != ZE_OK)
            ZIPERR(r, tempzip)
        tempzn += 4 + CENHEAD + z->nam + z->cext + z->com;
        n += z->len;
        t += z->siz;
        k++;
    }

    if (k == 0)
        co_await zipwarn("zip file empty", "");
    if (verbose)
        co_await zfprintf(mesg, "total bytes=%s, compressed=%s -> %d%% savings\n",
                          zip_fzofft(n, NULL, "u"), zip_fzofft(t, NULL, "u"), percent(n, t));

    t = tempzn - c; // compute length of central
    if ((r = co_await putend(k, t, c, zcomlen, zcomment)) != ZE_OK)
        ZIPERR(r, tempzip)

    if ((co_await zfclose(y)).is_err())
        ZIPERR(ZE_TEMP, tempzip)
    y = NULL;
    if (Task<void> c = close_fd(yfd))
        co_await c;

    // Replace the old archive with the new one, which is what makes an
    // interrupted run leave the original alone.
    if ((r = co_await replace(zipfile, tempzip)) != ZE_OK)
        ZIPERR(r, zipfile)
    free(tempzip);
    tempzip = NULL;

    co_return ZE_OK;
}

Task<i32> proc_main(Args args)
{
    zstdout = &File::stdout();
    zstderr = &File::stderr();
    mesg    = zstdout;

    // ^C is asked for, or the default action stands. zipmain removes the temp
    // archive on the way out, which is what leaves the original untouched.
    if (Task<Result<void>> t = sig_catch(SIG_INT))
        co_await t;

    int r = ZE_LOGIC;
    if (Task<int> t = zipmain(args.tail()))
        r = co_await t;

    // zip_fail() records without writing, because a plain function cannot
    // co_await one out. If nothing else reported the run, this does.
    if (r == ZE_OK && zip_fatal != ZE_OK) {
        r = zip_fatal;
        co_await zfprintf(zstderr, "\nzip error: %s (%s)\n", ZIPERRORS(r),
                          zip_fatal_h ? zip_fatal_h : "");
    }

    // The temp archive must not outlive a failed run.
    if (r != ZE_OK && tempzip != NULL) {
        if (y != NULL)
            co_await zfclose(y);
        co_await destroy(tempzip);
    }

    co_await zfflush(zstdout);
    co_await zfflush(zstderr);

    if (r == ZE_ABORT)
        co_return 130;
    co_return r;
}
