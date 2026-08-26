// Where /share/i18n is.
//
// The data ships in the same package as the binary, so it sits beside it under
// /pkg/store/iconv-<version>/, a path carrying a version the binary does not
// know. pkg writes /pkg/gen/<n>/bin/<cmd> as a symlink whose target is the
// store path, and readlink follows the directories on the way without
// following the leaf — so one syscall recovers the prefix.

#include "braam.h"
#include "fs/path.h"
#include "kernel/string.h"
#include "proc/io.h"

namespace {

// A namespace-scope global must be trivially destructible, so the prefix is a
// fixed buffer rather than a String.
constexpr usize PREFIX_MAX = 128;
char PREFIX[PREFIX_MAX];
usize PREFIX_LEN;

bool set_prefix(Str s)
{
    if (s.size() >= PREFIX_MAX)
        return false;
    for (usize i = 0; i < s.size(); i++)
        PREFIX[i] = s[i];
    PREFIX_LEN = s.size();
    return true;
}

// Does <prefix>/share/i18n/esdb exist? The prefix is only right if it does.
Task<bool> holds_data(Str prefix)
{
    String p;
    if (!p.assign(prefix) || !p.append("/share/i18n/esdb"))
        co_return false;
    Result<FileInfo> st = Err(Error::NoMemory);
    if (Task<Result<FileInfo>> t = stat_of(p.str()))
        st = co_await t;
    co_return st.is_ok();
}

} // namespace

Str citrus_prefix()
{
    return Str(PREFIX, PREFIX_LEN);
}

Task<void> citrus_prefix_init()
{
    // 1. What the caller said, which is also what the tests use.
    Str env = proc_env("ICONV_PREFIX");
    if (!env.empty() && set_prefix(env))
        co_return;

    // 2. The command's own store directory: resolve the link PATH found and
    // take the directory of its directory.
    Result<String> link = Err(Error::NoMemory);
    if (Task<Result<String>> t = read_link("/pkg/bin/iconv"))
        link = co_await t;
    if (link.is_ok()) {
        Str dir  = path_dirname(link.value().str()); // .../bin
        Str root = path_dirname(dir);                // .../iconv-<version>
        bool ok  = false;
        if (Task<bool> t = holds_data(root))
            ok = co_await t;
        if (ok && set_prefix(root))
            co_return;
    }

    // 3. Installed, but reached by a path of its own rather than through
    // /pkg/bin. The store is one directory and the name is a prefix of it.
    Result<Vec<DirEntry>> ents = Err(Error::NoMemory);
    if (Task<Result<Vec<DirEntry>>> t = list_dir("/pkg/store"))
        ents = co_await t;
    if (ents.is_ok()) {
        for (const DirEntry &e : ents.value()) {
            if (!e.name.str().starts_with("iconv-"))
                continue;
            String p;
            if (!p.assign("/pkg/store/") || !p.append(e.name.str()))
                continue;
            bool ok = false;
            if (Task<bool> t = holds_data(p.str()))
                ok = co_await t;
            if (ok && set_prefix(p.str()))
                co_return;
        }
    }

    // 4. Nothing found. An empty prefix names /share/i18n absolutely, which is
    // where a build that put the data in the image would keep it; when there
    // is none either, the first read says so against that path.
    PREFIX_LEN = 0;
}
