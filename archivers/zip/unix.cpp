// unix.cpp — unix/unix.c, by Mark Adler. What the OS answers about a file.
//
// Almost none of it survives as written. There is no stat with a mode in it,
// no uid or gid, no atime or ctime, and no opendir; there is list_dir, which
// hands back a name, a kind, a size and an mtime, and that is the whole of
// what the filesystem keeps. So the mode an entry carries is synthesized here
// and the UT and Ux extra fields are not written at all — there is nothing to
// put in them.

#include "crc32.h"
#include "revision.h"
#include "zip.h"

#define PATH_END '/'

// Bits for a mode the filesystem does not keep. An extractor on a real Unix
// wants something sensible, and these are what every other archiver invents.
#define ZMODE_FILE (S_IFREG | 0644)
#define ZMODE_DIR  (S_IFDIR | 0755)
#define ZMODE_LINK (S_IFLNK | 0777)

// If file *f does not exist, return 0. Else, return the file's last modified
// date and time as an MSDOS date and time, the date most significant so an
// unsigned compare orders absolute times. If a is not NULL store the
// attributes there, the high two bytes being the Unix ones and the low byte a
// mapping of that to DOS. If n is not NULL store the size. If t is not NULL
// store the times — all three the same one, since only mtime is kept.
Task<ulg> filetime(char *f, ulg *a, zoff_t *n, iztimes *t)
{
    // Not all systems allow stat'ing a name with a / appended, and this one
    // does not either.
    int len    = (int)strlen(f);
    char *name = (char *)malloc(len + 1);
    if (name == NULL) {
        zip_fail(ZE_MEM, "filetime");
        co_return 0;
    }
    strcpy(name, f);
    if (len > 0 && name[len - 1] == '/')
        name[len - 1] = '\0';

    // The link itself, not what it points at: a stored link is its target.
    Result<FileInfo> s = Err(Error::NoMemory);
    if (Task<Result<FileInfo>> tk = stat_of(Str(name, strlen(name)), false))
        s = co_await tk;
    free((zvoid *)name);
    if (s.is_err())
        co_return 0;

    ulg mode = s.value().kind == SYS_KIND_DIR    ? ZMODE_DIR
               : s.value().kind == SYS_KIND_LINK ? ZMODE_LINK
                                                 : ZMODE_FILE;

    if (a != NULL) {
        *a = (mode << 16) | !(mode & 0200);
        if ((mode & S_IFMT) == S_IFDIR)
            *a |= MSDOS_DIR_ATTR;
    }
    if (n != NULL)
        *n = (mode & S_IFMT) == S_IFREG ? (zoff_t)s.value().size : -1L;

    // A directory's mtime is 0 here — OPFS keeps none — and so is /proc's and
    // /dev's. zdostime() turns that into the DOS epoch.
    time_t mtime = (time_t)(s.value().mtime / 1000);
    if (t != NULL)
        t->atime = t->mtime = t->ctime = mtime;

    co_return zdostime(s.value().mtime);
}

// Upstream wrote a "UT" field with the access and modification times and a
// "Ux" with the uid and gid. The filesystem has none of the four, and a field
// full of invented values is worse than no field: an extractor would restore
// them. So an entry here carries the DOS stamp in its header and nothing else.
Task<int> set_extra_field(struct zlist far *z, iztimes *z_utim)
{
    z->extra = z->cextra = NULL;
    z->ext = z->cext = 0;
    co_return ZE_OK;
}

// Delete the directory *d, and what is under it.
Task<int> deletedir(char *d)
{
    Result<void> r = Err(Error::NoMemory);
    if (Task<Result<void>> t = remove_path(Str(d, strlen(d)), true))
        r = co_await t;
    co_return r.is_ok() ? ZE_OK : ZE_MISS;
}

// Convert the external file name to a zip file name, returning the malloc'ed
// string or NULL if not enough memory.
char *ex2in(char *x, int isdir, int *pdosflag)
{
    char *n;        // internal file name (malloc'ed)
    char *t = NULL; // shortened name
    int dosflag;

    dosflag = dosify;

    t = x;
    while (*t == '/')
        t++; // strip leading '/' chars to get a relative path
    while (*t == '.' && t[1] == '/')
        t += 2; // strip redundant leading "./" sections

    // Make changes, if any, to the copied name (leave original intact)
    if (!pathput)
        t = last(t, PATH_END);

    // Malloc space for internal name and copy it
    if ((n = (char *)malloc(strlen(t) + 1)) == NULL)
        return NULL;
    strcpy(n, t);

    if (dosify)
        msname(n);

    // Returned malloc'ed name
    if (pdosflag)
        *pdosflag = dosflag;

    if (isdir)
        return n; // avoid warning on unused variable
    return n;
}

// Convert the zip file name to an external file name, returning the malloc'ed
// string or NULL if not enough memory.
char *in2ex(char *n)
{
    char *x;

    if ((x = (char *)malloc(strlen(n) + 1)) == NULL)
        return NULL;
    strcpy(x, n);
    return x;
}

Task<void> version_local(void)
{
    co_await b_fprintf(mesg, "Compiled for Braam with clang, wasm32.\n\n");
}

// Process a name or sh expression to operate on (or exclude). Return an error
// code in the ZE_ class.
//
// Upstream walked a directory with opendir/readdir. list_dir hands back the
// whole listing in one syscall, and it never resolves a link — an entry says
// SYS_KIND_LINK whatever it points at — so recursing on SYS_KIND_DIR alone
// cannot follow a link out of the tree and needs no cycle guard. There are no
// FIFOs and no special files, so those two branches are gone with them.
Task<int> procname(char *n, int caseflag)
{
    char *a;             // path and name for recursion
    int m;               // matched flag
    char *p;             // path for recursion
    struct zlist far *z; // steps through zfiles list

    if (strcmp(n, "-") == 0) // if compressing stdin
        co_return co_await newname(n, 0, caseflag);

    Result<FileInfo> s = Err(Error::NoMemory);
    if (Task<Result<FileInfo>> t = stat_of(Str(n, strlen(n)), false))
        s = co_await t;

    if (s.is_err()) {
        // Not a file or directory -- search for shell expression in zip file
        p = ex2in(n, 0, (int *)NULL); // shouldn't affect matching chars
        m = 1;
        for (z = zfiles; z != NULL; z = z->nxt) {
            if (MATCH(p, z->iname, caseflag)) {
                z->mark = pcount ? filter(z->zname, caseflag) : 1;
                if (verbose)
                    co_await b_fprintf(mesg, "zip diagnostic: %scluding %s\n", z->mark ? "in" : "ex",
                                      z->name);
                m = 0;
            }
        }
        free((zvoid *)p);
        co_return m ? ZE_MISS : ZE_OK;
    }

    // Live name -- use if file, recurse if directory
    if (s.value().kind == SYS_KIND_FILE || s.value().kind == SYS_KIND_LINK) {
        // add or remove name of file
        if ((m = co_await newname(n, 0, caseflag)) != ZE_OK)
            co_return m;
    } else if (s.value().kind == SYS_KIND_DIR) {
        // Add trailing / to the directory name
        if ((p = (char *)malloc(strlen(n) + 2)) == NULL)
            co_return ZE_MEM;
        if (strcmp(n, ".") == 0) {
            *p = '\0'; // avoid "./" prefix and do not create zip entry
        } else {
            strcpy(p, n);
            a = p + strlen(p);
            if (a[-1] != '/')
                strcpy(a, "/");
            if (dirnames && (m = co_await newname(p, 1, caseflag)) != ZE_OK) {
                free((zvoid *)p);
                co_return m;
            }
        }

        // recurse into directory
        if (recurse) {
            Result<Vec<DirEntry>> d = Err(Error::NoMemory);
            if (Task<Result<Vec<DirEntry>>> t = list_dir(Str(n, strlen(n))))
                d = co_await t;
            if (d.is_ok()) {
                for (const DirEntry &e : d.value()) {
                    // "." and ".." are not in a listing here.
                    if ((a = (char *)malloc(strlen(p) + e.name.size() + 1)) == NULL) {
                        free((zvoid *)p);
                        co_return ZE_MEM;
                    }
                    strcpy(a, p);
                    memcpy(a + strlen(p), e.name.data(), e.name.size());
                    a[strlen(p) + e.name.size()] = 0;
                    if ((m = co_await procname(a, caseflag)) != ZE_OK) {
                        // recurse on name
                        if (m == ZE_MISS)
                            co_await zipwarn("name not matched: ", a);
                        else
                            co_await ziperr_msg(m, a);
                    }
                    free((zvoid *)a);
                    if (zip_fatal != ZE_OK) {
                        free((zvoid *)p);
                        co_return zip_fatal;
                    }
                }
            }
        }
        free((zvoid *)p);
    } else {
        co_await zipwarn("ignoring special file: ", n);
    }
    co_return ZE_OK;
}
