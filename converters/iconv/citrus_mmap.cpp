// _citrus_map_file, over a filesystem with no mmap. Upstream's opened, fstat'd
// and mapped; here a region is a heap block holding the whole file.
//
// This is also the one place a path gains its prefix. Every path in the library
// is built from the _PATH_ESDB and _PATH_CSMAPPER literals, and the package's
// store directory carries a version the binary does not know, so prepending
// here leaves all forty-one of those string literals exactly as upstream wrote
// them.

#include "citrus_mmap.h"

#include <errno.h>
#include <limits.h>
#include <string.h>

#include "braam.h"
#include "citrus_namespace.h"
#include "citrus_region.h"
#include "kernel/alloc.h"
#include "kernel/string.h"
#include "proc/io.h"

namespace {

// The four files every conversion reads, whatever it is converting. Held for
// the life of the process: together they are about 130 KB, and reading them
// once turns three of citrus's files from coroutines back into functions.
struct Cached {
    const char *path;
    void *head;
    usize size;
    bool tried;
};

Cached CACHE[] = {
    { _PATH_ESDB "/esdb.alias.db", nullptr, 0, false },
    { _PATH_ESDB "/esdb.dir.db", nullptr, 0, false },
    { _PATH_ESDB "/esdb.alias", nullptr, 0, false },
    { _PATH_ESDB "/esdb.dir", nullptr, 0, false },
    { _PATH_CSMAPPER "/charset.pivot.pvdb", nullptr, 0, false },
    { _PATH_CSMAPPER "/charset.pivot", nullptr, 0, false },
    { _PATH_CSMAPPER "/mapper.dir", nullptr, 0, false },
};

constexpr usize CACHE_N = sizeof(CACHE) / sizeof(CACHE[0]);

Cached *cached_of(const char *path)
{
    for (usize i = 0; i < CACHE_N; i++)
        if (strcmp(CACHE[i].path, path) == 0)
            return &CACHE[i];
    return nullptr;
}

// The prefix and the library's own path, joined. Every path here is well under
// PATH_MAX; a longer one is refused rather than truncated into another file.
bool full_path(const char *path, char *out, usize cap)
{
    Str prefix = citrus_prefix();
    usize n = prefix.size(), m = strlen(path);
    if (n + m + 1 > cap)
        return false;
    memcpy(out, prefix.data(), n);
    memcpy(out + n, path, m);
    out[n + m] = 0;
    return true;
}

} // namespace

// A whole file into one block. stat_fd sizes it, so there is one allocation
// however long the read takes.
Task<int> _citrus_load_file(struct _citrus_region *r, const char *path)
{
    _region_init(r, nullptr, 0);

    char full[PATH_MAX];
    if (!full_path(path, full, sizeof(full)))
        co_return ENAMETOOLONG;

    Result<i32> fd = Err(Error::NoMemory);
    if (Task<Result<i32>> t = open_at(Str(full, strlen(full)), SYS_O_READ))
        fd = co_await t;
    if (fd.is_err())
        co_return fd.error() == Error::NotFound ? ENOENT : EINVAL;

    int ret             = 0;
    void *block         = nullptr;
    usize want          = 0;
    Result<FileInfo> st = Err(Error::NoMemory);

    if (Task<Result<FileInfo>> t = stat_fd(u32(fd.value())))
        st = co_await t;
    if (st.is_err())
        ret = EINVAL;
    else
        want = (usize)st.value().size;

    // An empty file is a region of nothing, not a failure.
    if (!ret && want > 0) {
        block = heap_alloc(want);
        if (!block)
            ret = ENOMEM;
    }

    if (block) {
        usize got = 0;
        while (got < want) {
            Result<String> chunk = Err(Error::NoMemory);
            if (Task<Result<String>> t = read_some(u32(fd.value()), u32(want - got)))
                chunk = co_await t;
            if (chunk.is_err()) {
                // Closed before the size stat promised: a short file is not
                // what the caller asked for.
                ret = chunk.error() == Error::Closed ? EINVAL : EIO;
                break;
            }
            usize n = chunk.value().size();
            if (n == 0) {
                ret = EINVAL;
                break;
            }
            memcpy((char *)block + got, chunk.value().data(), n);
            got += n;
        }

        if (ret)
            heap_free(block);
        else
            _region_init(r, block, want);
    }

    if (Task<void> c = close_fd(u32(fd.value())))
        co_await c;
    co_return ret;
}

// Reads the four index files, once. A path that is not there is remembered as
// absent, so the misses cost nothing later — charset.alias does not ship, and
// citrus asks for it at every csmapper open.
Task<Result<void>> citrus_preload()
{
    for (usize i = 0; i < CACHE_N; i++) {
        Cached &c = CACHE[i];
        _citrus_region r;
        int ret = 1;
        if (Task<int> t = _citrus_load_file(&r, c.path))
            ret = co_await t;
        c.tried = true;
        if (ret == 0) {
            c.head = _region_head(&r);
            c.size = _region_size(&r);
        }
    }

    // The two .db forms are what the readers try first; without them nothing
    // resolves at all, and the fallbacks are text of the same content.
    if (!CACHE[0].head && !CACHE[2].head)
        co_return Err(Error::NotFound);
    co_return Result<void>();
}

// mapper.dir is read whatever the conversion, so "is it there" is a question
// the cache can answer without a syscall.
extern "C" int citrus_have_file(const char *path)
{
    Cached *c = cached_of(path);
    return c && c->head ? 0 : ENOENT;
}

// Upstream's signature, answered out of the cache. A path that is not one of
// the four is a programming error rather than a missing file: everything else
// goes through _citrus_load_file.
extern "C" int _citrus_map_file(struct _citrus_region *__restrict r, const char *__restrict path)
{
    _region_init(r, nullptr, 0);

    Cached *c = cached_of(path);
    if (!c || !c->head)
        return ENOENT;

    _region_init(r, c->head, c->size);
    return 0;
}

extern "C" void _citrus_unmap_file(struct _citrus_region *r)
{
    void *head = _region_head(r);
    if (head) {
        bool borrowed = false;
        for (usize i = 0; i < CACHE_N; i++)
            if (CACHE[i].head == head)
                borrowed = true;
        if (!borrowed)
            heap_free(head);
    }
    _region_init(r, nullptr, 0);
}
