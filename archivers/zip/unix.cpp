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
    co_await zfprintf(mesg, "Compiled for Braam with clang, wasm32.\n\n");
}
