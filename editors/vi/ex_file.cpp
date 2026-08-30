/*
 * The file system calls, in the shapes ex expects them.
 *
 * Upstream called open, close, read, write, lseek, stat and access directly,
 * and checked ex_errno afterwards. Each is a syscall here and every syscall is
 * awaited, so each becomes a Task; what each answers is upstream's own
 * convention -- a descriptor or -1, a count or -1 -- with ex_errno set beside it,
 * so that filioerr() and syserror() still have something to report.
 */
#include "ex.h"
#include "kernel/str.h"

/*
 * A path as a Str. ex keeps names in fixed char arrays and NUL-terminates
 * them, so this is a length rather than a copy.
 */
static Str path_of(char *p)
{
    return (Str(p, strlen(p)));
}

/* Modes are upstream's: 0 read, 1 write, 2 read/write. */
Task<int> ex_open(char *path, int mode)
{
    u32 flags     = mode == 0 ? SYS_O_READ : mode == 1 ? SYS_O_WRITE : (SYS_O_READ | SYS_O_WRITE);
    Result<i32> r = Err(Error::NoMemory);

    if (Task<Result<i32>> t = open_at(path_of(path), flags))
        r = co_await t;
    if (r.is_err()) {
        ex_errno = int(r.error());
        co_return (-1);
    }
    co_return (res_of(r));
}

Task<int> ex_creat(char *path)
{
    Result<i32> r = Err(Error::NoMemory);

    if (Task<Result<i32>> t = open_at(path_of(path), SYS_O_WRITE | SYS_O_CREATE | SYS_O_TRUNC))
        r = co_await t;
    if (r.is_err()) {
        ex_errno = int(r.error());
        co_return (-1);
    }
    co_return (res_of(r));
}

Task<void> ex_close(int fd)
{
    if (fd < 0)
        co_return;
    if (Task<void> t = close_fd(fd))
        co_await t;
}

/*
 * Upstream read into a caller's buffer; read_some answers a String, so this
 * copies. The buffers are LBSIZE at most and a copy of a kilobyte is nothing
 * beside the two postMessage hops the syscall already cost.
 */
Task<int> ex_read(int fd, char *buf, int n)
{
    Result<String> r = Err(Error::NoMemory);

    if (Task<Result<String>> t = read_some(fd, (u32)n))
        r = co_await t;
    if (r.is_err()) {
        if (r.error() == Error::Closed)
            co_return (0);
        ex_errno = int(r.error());
        co_return (-1);
    }
    {
        Str got = res_of(r).str();
        int k   = (int)got.size();

        if (k > n)
            k = n;
        memcpy(buf, got.data(), k);
        co_return (k);
    }
}

Task<int> ex_write(int fd, char *buf, int n)
{
    Result<void> r = Err(Error::NoMemory);

    if (Task<Result<void>> t = write_all(fd, Str(buf, n)))
        r = co_await t;
    if (r.is_err()) {
        ex_errno = int(r.error());
        co_return (-1);
    }
    co_return (n);
}

Task<long> ex_seek(int fd, long off, int whence)
{
    Result<u64> r = Err(Error::NoMemory);

    if (Task<Result<u64>> t = seek_fd(fd, (i64)off, (u32)whence))
        r = co_await t;
    if (r.is_err()) {
        ex_errno = int(r.error());
        co_return (-1);
    }
    co_return ((long)res_of(r));
}

/*
 * stat, in the one shape ex uses it: does it exist, is it a directory, and how
 * big is it. Upstream also asked for the mode bits and the device, to warn
 * about writing over a file that was not the one it read and to keep a temp
 * file off another filesystem; neither question has an answer here.
 */
Task<int> ex_stat(char *path, struct exstat *sb)
{
    Result<FileInfo> r = Err(Error::NoMemory);

    if (Task<Result<FileInfo>> t = stat_of(path_of(path)))
        r = co_await t;
    if (r.is_err()) {
        ex_errno = int(r.error());
        co_return (-1);
    }
    sb->st_size  = (long)res_of(r).size;
    sb->st_isdir = res_of(r).kind == SYS_KIND_DIR;
    co_return (0);
}

Task<int> ex_fstat(int fd, struct exstat *sb)
{
    Result<FileInfo> r = Err(Error::NoMemory);

    if (Task<Result<FileInfo>> t = stat_fd(fd))
        r = co_await t;
    if (r.is_err()) {
        ex_errno = int(r.error());
        co_return (-1);
    }
    sb->st_size  = (long)res_of(r).size;
    sb->st_isdir = res_of(r).kind == SYS_KIND_DIR;
    co_return (0);
}
