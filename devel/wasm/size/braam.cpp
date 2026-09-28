// The Braam front end: reads each file the command line names, hands its bytes
// to the core, and writes what comes back. Only this file awaits.
#include "driver.h"
#include "files.h"
#include "kernel/alloc.h"

namespace {

const char USAGE[] =
    "usage: size [options] [file...]\n"
    "  -B, --format=berkeley    text, data and bss per module (the default)\n"
    "  -A, --format=sysv        every section, its size and address\n"
    "  -m, --format=darwin      as -B, for wasm\n"
    "  -d, -o, -x, --radix=10|8|16\n"
    "                           print sizes in decimal, octal or hex\n"
    "  -t, --totals             add up every module; -B only\n"
    "With no file, a.out is read; - is standard input.\n";

// Everything the front end holds, off the coroutine frame.
struct Front {
    SizeArgs args;
    Sizer sizer;
    Diag diag;
    String in; // standard input, read whole
    bool cancelled = false;
};

void failed(Front &s, Str path, Error why)
{
    if (why == Error::Cancelled) {
        s.cancelled = true;
        return;
    }
    Out m;
    m.put("cannot open ").put(path).put(": ").put(error_name(why));
    s.diag.error(m.str());
}

// What is ready for stdout and stderr, written now, so an error stands among
// the files where it happened.
Task<void> flush(Front &s)
{
    if (s.sizer.out.oom || s.diag.text.oom) {
        s.sizer.out.clear();
        s.diag.text.clear();
        s.diag.error("out of memory");
    }
    co_await say(SYS_STDOUT, s.sizer.out.str());
    co_await say(SYS_STDERR, s.diag.text.str());
    s.sizer.out.clear();
    s.diag.text.clear();
}

Task<Result<void>> read_stdin(Front &s)
{
    s.in.clear();
    for (;;) {
        Task<Result<String>> t = read_chunk(SYS_STDIN);
        if (!t)
            co_return Err(Error::NoMemory);
        Result<String> r = co_await t;
        if (r.is_err() && r.error() == Error::Closed)
            co_return Result<void>();
        if (r.is_err())
            co_return Err(r.error());
        if (!s.in.append(r.value().str()))
            co_return Err(Error::NoMemory);
    }
}

Task<void> size_one(Front &s, Str path)
{
    if (path == "-") {
        Result<void> r = co_await read_stdin(s);
        if (r.is_err()) {
            failed(s, path, r.error());
            co_return;
        }
        size_file(s.sizer, "<stdin>", Bytes(reinterpret_cast<const u8 *>(s.in.data()), s.in.size()),
                  s.diag);
        co_return;
    }
    Result<String> r = co_await slurp(path);
    if (r.is_err()) {
        failed(s, path, r.error());
        co_return;
    }
    const String &f = r.value();
    size_file(s.sizer, path, Bytes(reinterpret_cast<const u8 *>(f.data()), f.size()), s.diag);
}

Task<i32> run(Front &s, Args args)
{
    s.diag.tool  = "size";
    s.diag.limit = 0;
    Vec<Str> words;
    bool ok = true;
    for (usize i = 1; ok && i < args.size(); i++)
        if (!words.push(args[i])) {
            s.diag.error("out of memory");
            ok = false;
        }
    // A bad --format or --radix is reported, and the files are sized anyway.
    if (ok && parse_args(words, s.args, s.diag)) {
        if (s.args.help) {
            co_await say(SYS_STDOUT, USAGE);
            co_return 0;
        }
        s.sizer.c = s.args.size;
        for (Str path : s.args.inputs) {
            co_await size_one(s, path);
            if (s.cancelled)
                break;
            co_await flush(s);
        }
        if (!s.cancelled)
            size_totals(s.sizer);
    }
    co_await flush(s);
    if (s.cancelled)
        co_return 130;
    co_return s.diag.errors ? 1 : 0;
}

} // namespace

Task<i32> proc_main(Args args)
{
    Front *s = heap_new<Front>();
    if (!s)
        co_return 1;
    i32 status = co_await run(*s, args);
    heap_delete(s);
    co_return status;
}
