/*	EPATH.C
 *
 *	Where the package's own share directory is.
 *
 *	The data ships beside the binary under /pkg/store/uemacs-<version>/, a
 *	path carrying a version the binary does not know.  pkg writes
 *	/pkg/gen/<n>/bin/em as a symlink into the store, and readlink does not
 *	follow the leaf -- so one syscall recovers the prefix.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "estruct.h"
#include "globals.h"
#include "efunc.h"

#include "fs/path.h"
#include "proc/io.h"

char datadir[NFILEN]; /* the package's share directory, empty for none */

/* A candidate prefix and the file probed under it.  At file scope: a
   coroutine's locals live in the frame, and 512 bytes of them cost a span. */
static char cand[NFILEN];
static char spec[NFILEN];

/* A Str into a buffer, NUL-terminated.  FALSE if it does not fit. */
static int put_str(char *buf, unsigned size, Str s)
{
    if (s.size() >= size)
        return FALSE;
    memcpy(buf, s.data(), s.size());
    buf[s.size()] = 0;
    return TRUE;
}

/* Does <prefix>/share hold emacs.hlp?  Sets datadir when it does. */
static Task<int> holds_data(Str prefix)
{
    Result<FileInfo> st = Err(Error::NoMemory);

    if (!put_str(datadir, sizeof(datadir), prefix))
        co_return FALSE;
    if (strlen(datadir) + sizeof("/share/emacs.hlp") > NFILEN)
        co_return FALSE;
    strcat(datadir, "/share");
    strcpy(spec, datadir);
    strcat(spec, "/emacs.hlp");
    if (Task<Result<FileInfo>> t = stat_of(Str(spec, strlen(spec))))
        st = co_await t;
    co_return st.is_ok() ? TRUE : FALSE;
}

Task<void> epath_init(void)
{
    int ok = FALSE;

    /* 1. What the caller said. */
    char *env = getenv("EMACS_PREFIX");
    if (env != NULL && *env != 0) {
        if (Task<int> t = holds_data(Str(env, strlen(env))))
            ok = co_await t;
        if (ok)
            co_return;
    }

    /* 2. Resolve the link PATH found; the store root is two directories up. */
    Result<String> link = Err(Error::NoMemory);
    if (Task<Result<String>> t = read_link("/pkg/bin/em"))
        link = co_await t;
    if (link.is_ok()) {
        Str dir  = path_dirname(link.value().str()); /* .../bin */
        Str root = path_dirname(dir);                /* .../uemacs-<version> */
        if (Task<int> t = holds_data(root))
            ok = co_await t;
        if (ok)
            co_return;
    }

    /* 3. Reached by a path of its own rather than through /pkg/bin: scan the
       store for the one directory the name is a prefix of. */
    Result<Vec<DirEntry>> ents = Err(Error::NoMemory);
    if (Task<Result<Vec<DirEntry>>> t = list_dir("/pkg/store"))
        ents = co_await t;
    if (ents.is_ok()) {
        for (const DirEntry &e : ents.value()) {
            if (!e.name.str().starts_with("uemacs-"))
                continue;
            strcpy(cand, "/pkg/store/");
            if (!put_str(cand + strlen(cand), sizeof(cand) - strlen(cand), e.name.str()))
                continue;
            if (Task<int> t = holds_data(Str(cand, strlen(cand))))
                ok = co_await t;
            if (ok)
                co_return;
        }
    }

    /* 4. Nothing found: lookup_file() skips the probe. */
    datadir[0] = 0;
}
