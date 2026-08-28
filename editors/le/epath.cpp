// Where the package's own share directory is.
//
// The data ships beside the binary under /pkg/store/le-<version>/, a path
// carrying a version the binary does not know. pkg writes /pkg/gen/<n>/bin/le
// as a symlink into the store, and readlink does not follow the leaf -- so one
// syscall recovers the prefix.

#include "epath.h"

#include "braam.h"
#include "fs/path.h"
#include "proc/io.h"

enum { PATHMAX = 256 };

char datadir[PATHMAX];

// At file scope: a coroutine's locals live in the frame, and 512 bytes of them
// cost a span.
static char cand[PATHMAX];
static char spec[PATHMAX];

// A Str into a buffer, NUL-terminated. False if it does not fit.
static bool put_str(char *buf, unsigned size, Str s)
{
    if (s.size() >= size)
        return false;
    memcpy(buf, s.data(), s.size());
    buf[s.size()] = 0;
    return true;
}

// Does <prefix>/share hold le.hlp? Sets datadir when it does.
static Task<bool> holds_data(Str prefix)
{
    Result<FileInfo> st = Err(Error::NoMemory);

    if (!put_str(datadir, sizeof(datadir), prefix))
        co_return false;
    if (strlen(datadir) + sizeof("/share/le.hlp") > sizeof(datadir))
        co_return false;
    strcat(datadir, "/share");
    strcpy(spec, datadir);
    strcat(spec, "/le.hlp");
    if (Task<Result<FileInfo>> t = stat_of(Str(spec, strlen(spec))))
        st = co_await t;
    co_return st.is_ok();
}

Task<void> epath_init()
{
    bool ok = false;

    // 1. What the caller said.
    char *env = getenv("LE_PREFIX");
    if (env != NULL && *env != 0) {
        if (Task<bool> t = holds_data(Str(env, strlen(env))))
            ok = co_await t;
        if (ok)
            co_return;
    }

    // 2. Resolve the link PATH found; the store root is two directories up.
    Result<String> link = Err(Error::NoMemory);
    if (Task<Result<String>> t = read_link("/pkg/bin/le"))
        link = co_await t;
    if (link.is_ok()) {
        Str dir  = path_dirname(link.value().str()); // .../bin
        Str root = path_dirname(dir);                // .../le-<version>
        if (Task<bool> t = holds_data(root))
            ok = co_await t;
        if (ok)
            co_return;
    }

    // 3. Reached by a path of its own rather than through /pkg/bin: scan the
    //    store for the one directory the name is a prefix of.
    Result<Vec<DirEntry>> ents = Err(Error::NoMemory);
    if (Task<Result<Vec<DirEntry>>> t = list_dir("/pkg/store"))
        ents = co_await t;
    if (ents.is_ok()) {
        for (const DirEntry &e : ents.value()) {
            if (!e.name.str().starts_with("le-"))
                continue;
            strcpy(cand, "/pkg/store/");
            if (!put_str(cand + strlen(cand), sizeof(cand) - strlen(cand), e.name.str()))
                continue;
            if (Task<bool> t = holds_data(Str(cand, strlen(cand))))
                ok = co_await t;
            if (ok)
                co_return;
        }
    }

    // 4. Nothing found: every caller skips the probe.
    datadir[0] = 0;
}

const char *datafile(char *buf, unsigned size, const char *name)
{
    if (datadir[0] == 0 || strlen(datadir) + 1 + strlen(name) + 1 > size) {
        buf[0] = 0;
        return buf;
    }
    strcpy(buf, datadir);
    strcat(buf, "/");
    strcat(buf, name);
    return buf;
}
