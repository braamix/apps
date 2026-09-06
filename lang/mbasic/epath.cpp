// Where the package's own examples are.
//
// They ship under /pkg/store/mbasic-<version>/share/, a path carrying a version
// the binary does not know. pkg writes /pkg/gen/<n>/bin/mbasic as a symlink into
// the store, and readlink does not follow the leaf -- so one syscall recovers
// the prefix. LOAD falls back on this for a bare name; SAVE does not, because
// the store is read-only.
#include "epath.h"

#include "fs/path.h"
#include "kernel/alloc.h"
#include "proc/io.h"

namespace {

// A namespace-scope String is not trivially destructible, and __cxa_atexit does
// not exist here. Built on first use and never freed.
String *datadir;

Str STORE  = "/pkg/store";
Str PREFIX = "mbasic-";

// Is <root>/share a directory? Keeps it when it is.
Task<bool> holds_data(Str root)
{
    if (!datadir)
        datadir = heap_new<String>();
    if (!datadir || !datadir->assign(root) || !datadir->append("/share"))
        co_return false;

    Result<FileInfo> st = Err(Error::NoMemory);
    if (Task<Result<FileInfo>> t = stat_of(datadir->str()))
        st = co_await t;
    if (st.is_ok() && st.value().kind == SYS_KIND_DIR)
        co_return true;
    datadir->clear();
    co_return false;
}

} // namespace

Task<void> epath_init()
{
    // 1. Resolve the link PATH found; the store root is two directories up.
    Result<String> link = Err(Error::NoMemory);
    if (Task<Result<String>> t = read_link("/pkg/bin/mbasic"))
        link = co_await t;
    if (link.is_ok()) {
        Str dir = path_dirname(link.value().str()); // .../bin
        if (Task<bool> t = holds_data(path_dirname(dir)))
            if (co_await t)
                co_return;
    }

    // 2. Reached by a path of its own rather than through /pkg/bin: scan the
    //    store for the one directory the name is a prefix of.
    Result<Vec<DirEntry>> ents = Err(Error::NoMemory);
    if (Task<Result<Vec<DirEntry>>> t = list_dir(STORE))
        ents = co_await t;
    if (ents.is_err())
        co_return;

    String cand;
    for (const DirEntry &e : ents.value()) {
        if (!e.name.str().starts_with(PREFIX))
            continue;
        if (path_join(STORE, e.name.str(), cand).is_err())
            continue;
        if (Task<bool> t = holds_data(cand.str()))
            if (co_await t)
                co_return;
    }
}

bool epath_file(Str name, String &out)
{
    if (!datadir || datadir->size() == 0 || name.size() == 0)
        return false;
    if (name.contains("/")) // a path of the caller's own is taken as given
        return false;
    return path_join(datadir->str(), name, out).is_ok();
}
