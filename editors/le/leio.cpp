// The file syscalls. See leio.h.

#include "leio.h"

#include "braam.h"
#include "proc/io.h"

int errno;

namespace {

int fail(Error e)
{
    errno = int(e);
    return -1;
}

Str str_of(const char *s)
{
    return Str(s, strlen(s));
}

// The path, as an inode number. OPFS has none, and st_ino is what LE's
// SameFile() compares -- with a constant there every file would look like
// every other, and the "changed out of the editor" warning fires on the first
// save. A hash of the path says what SameFile means.
u64 ino_of(Str path)
{
    u64 h = 1469598103934665603ull; // FNV-1a
    for (usize i = 0; i < path.size(); i++) {
        h ^= u8(path[i]);
        h *= 1099511628211ull;
    }
    return h;
}

// FileInfo is kind, size and mtime; the rest of struct stat was never filled
// from anything the VFS keeps.
void fill(struct stat *st, const FileInfo &fi)
{
    st->st_mode  = fi.kind == SYS_KIND_DIR    ? (S_IFDIR | 0755)
                   : fi.kind == SYS_KIND_LINK ? (S_IFLNK | 0777)
                                              : (S_IFREG | 0644);
    st->st_size  = (off_t)fi.size;
    st->st_mtime = (time_t)(fi.mtime / 1000);
    st->st_ino   = 0;
    st->st_dev   = 1;
}

Task<int> stat_into(Str path, struct stat *st, bool follow)
{
    Result<FileInfo> r = Err(Error::NoMemory);

    if (Task<Result<FileInfo>> t = stat_of(path, follow))
        r = co_await t;
    if (r.is_err())
        co_return fail(r.error());
    fill(st, r.value());
    st->st_ino = ino_of(path);
    co_return 0;
}

} // namespace

Task<int> le_open(const char *path, int flags, mode_t)
{
    u32 f = 0;

    switch (flags & O_ACCMODE) {
    case O_RDONLY:
        f = SYS_O_READ;
        break;
    case O_WRONLY:
        f = SYS_O_WRITE;
        break;
    default:
        f = SYS_O_READ | SYS_O_WRITE;
        break;
    }
    if (flags & O_CREAT)
        f |= SYS_O_CREATE;
    if (flags & O_TRUNC)
        f |= SYS_O_TRUNC;
    if (flags & O_APPEND)
        f |= SYS_O_APPEND;
    if (flags & O_EXCL)
        f |= SYS_O_EXCL;

    Result<i32> r = Err(Error::NoMemory);
    if (Task<Result<i32>> t = open_at(str_of(path), f))
        r = co_await t;
    co_return r.is_err() ? fail(r.error()) : r.value();
}

// close_fd answers nothing: a close cannot fail in a way a caller could act on.
Task<int> le_close(int fd)
{
    if (fd < 0)
        co_return 0;
    if (Task<void> t = close_fd((u32)fd))
        co_await t;
    co_return 0;
}

Task<ssize_t> le_read(int fd, void *buf, size_t n)
{
    Result<String> r = Err(Error::NoMemory);

    if (n == 0)
        co_return 0;
    if (Task<Result<String>> t = read_some((u32)fd, (u32)n))
        r = co_await t;
    if (r.is_err()) {
        // End of input is zero bytes, as read(2) has it.
        if (r.error() == Error::Closed)
            co_return 0;
        co_return fail(r.error());
    }
    memcpy(buf, r.value().data(), r.value().size());
    co_return (ssize_t) r.value().size();
}

Task<ssize_t> le_write(int fd, const void *buf, size_t n)
{
    Result<void> r = Err(Error::NoMemory);

    if (Task<Result<void>> t = write_all((u32)fd, Str((const char *)buf, n)))
        r = co_await t;
    co_return r.is_err() ? fail(r.error()) : (ssize_t)n;
}

Task<int> le_write_all(int fd, const void *buf, size_t n)
{
    co_return (co_await le_write(fd, buf, n)) < 0 ? -1 : 0;
}

Task<off_t> le_lseek(int fd, off_t off, int whence)
{
    Result<u64> r = Err(Error::NoMemory);

    if (Task<Result<u64>> t = seek_fd((u32)fd, (i64)off, (u32)whence))
        r = co_await t;
    co_return r.is_err() ? (off_t)fail(r.error()) : (off_t)r.value();
}

Task<int> le_ftruncate(int fd, off_t n)
{
    Result<void> r = Err(Error::NoMemory);

    if (Task<Result<void>> t = truncate_fd((u32)fd, (u64)n))
        r = co_await t;
    co_return r.is_err() ? fail(r.error()) : 0;
}

Task<int> le_stat(const char *path, struct stat *st)
{
    co_return co_await stat_into(str_of(path), st, true);
}

Task<int> le_lstat(const char *path, struct stat *st)
{
    co_return co_await stat_into(str_of(path), st, false);
}

// No path, so no inode: the one caller compares a mode, not an identity.
Task<int> le_fstat(int fd, struct stat *st)
{
    Result<FileInfo> r = Err(Error::NoMemory);

    if (Task<Result<FileInfo>> t = stat_fd((u32)fd))
        r = co_await t;
    if (r.is_err())
        co_return fail(r.error());
    fill(st, r.value());
    co_return 0;
}

Task<int> le_unlink(const char *path)
{
    Result<void> r = Err(Error::NoMemory);

    if (Task<Result<void>> t = remove_path(str_of(path), false))
        r = co_await t;
    co_return r.is_err() ? fail(r.error()) : 0;
}

Task<int> le_rename(const char *from, const char *to)
{
    Result<void> r = Err(Error::NoMemory);

    if (Task<Result<void>> t = rename_path(str_of(from), str_of(to)))
        r = co_await t;
    co_return r.is_err() ? fail(r.error()) : 0;
}

Task<int> le_mkdir(const char *path, mode_t)
{
    Result<void> r = Err(Error::NoMemory);

    if (Task<Result<void>> t = make_dir(str_of(path)))
        r = co_await t;
    co_return r.is_err() ? fail(r.error()) : 0;
}

// There are no permission bits to test, so this answers whether the path is
// there -- which is what every caller here asks it.
Task<int> le_access(const char *path, int)
{
    struct stat st;

    co_return co_await le_stat(path, &st);
}
