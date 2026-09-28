// The Braam front end: reads each file the command line names, hands its bytes
// to the core, and writes what comes back as it comes. Only this file awaits.
#include "driver.h"
#include "files.h"
#include "kernel/alloc.h"

namespace {

const char USAGE[] =
    "usage: disasm [options] [file...]\n"
    "  -d, --disassemble        the code (the default)\n"
    "  -D, --disassemble-all    the code, and the data as bytes\n"
    "  -r, --reloc              an object's relocations, where they apply\n"
    "  -C, --demangle           demangle C++ names; --no-demangle undoes it\n"
    "  --no-show-raw-insn       no instruction bytes\n"
    "With no file, a.out is read; - is standard input.\n";

// Everything the front end holds, off the coroutine frame.
struct Front {
    DisasmArgs args;
    Disasm d;
    Diag diag;
    String in; // the file in hand, read whole
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
    if (s.d.out.oom || s.diag.text.oom) {
        s.d.out.clear();
        s.diag.text.clear();
        s.diag.error("out of memory");
    }
    Result<void> r = co_await say(SYS_STDOUT, s.d.out.str());
    if (r.is_err() && r.error() == Error::Cancelled)
        s.cancelled = true;
    co_await say(SYS_STDERR, s.diag.text.str());
    s.d.out.clear();
    s.diag.text.clear();
}

Task<Result<String>> read_stdin()
{
    String in;
    for (;;) {
        Task<Result<String>> t = read_chunk(SYS_STDIN);
        if (!t)
            co_return Err(Error::NoMemory);
        Result<String> r = co_await t;
        if (r.is_err() && r.error() == Error::Closed)
            co_return move(in);
        if (r.is_err())
            co_return Err(r.error());
        if (!in.append(r.value().str()))
            co_return Err(Error::NoMemory);
    }
}

// A file, written out a part at a time; a large one is megabytes of text.
Task<void> disasm_one(Front &s, Str path)
{
    bool in          = path == "-";
    Result<String> r = in ? co_await read_stdin() : co_await slurp(path);
    if (r.is_err()) {
        failed(s, path, r.error());
        co_return;
    }
    s.in = move(r.value());
    disasm_file(s.d, in ? "<stdin>"_s : path,
                Bytes(reinterpret_cast<const u8 *>(s.in.data()), s.in.size()), s.diag);
    while (!s.cancelled && disasm_more(s.d, s.diag))
        if (s.d.out.str().size() >= 32768 || !s.diag.text.str().empty())
            co_await flush(s);
    disasm_end(s.d);
}

Task<i32> run(Front &s, Args args)
{
    s.diag.tool  = "disasm";
    s.diag.limit = 0;
    Vec<Str> words;
    bool ok = true;
    for (usize i = 1; ok && i < args.size(); i++)
        if (!words.push(args[i])) {
            s.diag.error("out of memory");
            ok = false;
        }
    if (ok && parse_args(words, s.args, s.diag)) {
        if (s.args.help) {
            co_await say(SYS_STDOUT, USAGE);
            co_return 0;
        }
        s.d.c = s.args.c;
        for (Str path : s.args.inputs) {
            co_await disasm_one(s, path);
            if (s.cancelled)
                break;
            co_await flush(s);
        }
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
