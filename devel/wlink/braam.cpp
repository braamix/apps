// The Braam front end: reads what the command line names, hands the bytes to
// the core, and writes what comes back. Only this file awaits.
//
// So far: wlink --dump <object or archive>..., with @file naming more.
#include "dump.h"
#include "kernel/alloc.h"
#include "proc/io.h"

namespace {

// Everything the front end holds, off the coroutine frame.
struct State {
    Vec<String> files; // response files and inputs, kept alive
    Vec<Str> words;
    Out out;
    Out err;
};

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

bool space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

// Splits a response file into words at white space.
bool split(Str text, Vec<Str> &words)
{
    usize i = 0;
    while (i < text.size()) {
        while (i < text.size() && space(text[i]))
            i++;
        usize from = i;
        while (i < text.size() && !space(text[i]))
            i++;
        if (i > from && !words.push(text.substr(from, i - from)))
            return false;
    }
    return true;
}

Task<i32> fail(State &s, Str what, Error why)
{
    s.err.put("wlink: ").put(what).put(": ").put(error_name(why)).put('\n');
    co_await say(SYS_STDERR, s.err.str());
    co_return why == Error::Cancelled ? 130 : 1;
}

Task<i32> run(State &s, Args args)
{
    for (usize i = 1; i < args.size(); i++) {
        Str a = args[i];
        if (!a.starts_with("@")) {
            s.words.push(a);
            continue;
        }
        Result<String> r = co_await slurp(a.substr(1));
        if (r.is_err())
            co_return co_await fail(s, a.substr(1), r.error());
        if (!s.files.push(move(r.value())) || !split(s.files.back().str(), s.words))
            co_return co_await fail(s, a, Error::NoMemory);
    }

    if (s.words.size() < 2 || s.words[0] != "--dump") {
        co_await say(SYS_STDERR, "usage: wlink --dump <object or archive>...\n");
        co_return 1;
    }

    for (usize i = 1; i < s.words.size(); i++) {
        Str path         = s.words[i];
        Result<String> r = co_await slurp(path);
        if (r.is_err())
            co_return co_await fail(s, path, r.error());
        String &file = r.value();
        Bytes bytes(reinterpret_cast<const u8 *>(file.data()), file.size());
        s.out.clear();
        bool ok = dump_file(path, bytes, s.out, s.err);
        if (s.out.oom || s.err.oom)
            co_return co_await fail(s, path, Error::NoMemory);
        Result<void> w = co_await say(SYS_STDOUT, s.out.str());
        if (w.is_err())
            co_return w.error() == Error::Cancelled ? 130 : 1;
        if (!ok) {
            s.err.put('\n');
            s.out.clear();
            s.out.put("wlink: ").put(s.err.str());
            co_await say(SYS_STDERR, s.out.str());
            co_return 1;
        }
    }
    co_return 0;
}

} // namespace

Task<i32> proc_main(Args args)
{
    State *s = heap_new<State>();
    if (!s)
        co_return 1;
    i32 status = co_await run(*s, args);
    heap_delete(s);
    co_return status;
}
