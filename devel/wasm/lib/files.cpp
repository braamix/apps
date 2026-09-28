#include "files.h"

Task<Result<void>> say(u32 fd, Str s)
{
    if (s.empty())
        co_return Result<void>();
    Task<Result<void>> t = write_all(fd, s);
    if (!t)
        co_return Err(Error::NoMemory);
    co_return co_await t;
}

Task<Result<String>> slurp(Str path)
{
    Task<Result<String>> t = read_file(path);
    if (!t)
        co_return Err(Error::NoMemory);
    co_return co_await t;
}

Task<Result<void>> spill(Str path, Str s, u32 flags)
{
    Task<Result<i32>> o = open_at(path, SYS_O_WRITE | SYS_O_CREATE | flags);
    if (!o)
        co_return Err(Error::NoMemory);
    Result<i32> fd = co_await o;
    if (fd.is_err())
        co_return Err(fd.error());
    Result<void> w = co_await say(u32(fd.value()), s);
    if (Task<void> c = close_fd(u32(fd.value())))
        co_await c;
    co_return w;
}
