// The Braam front end: reads each file the command line names, hands its bytes
// to the core, and writes what comes back. Only this file awaits.
#include "driver.h"
#include "files.h"
#include "kernel/alloc.h"
#include "proc/opt.h"
#include "proc/usage.h"

namespace {

constexpr Str USAGE =
    "Usage:\n"
    "    nm [<options>] <file>...\n"
    "Options:\n"
    "    -A, -o, --print-file-name  name the file on every line\n"
    "    -f <format>, --format=<format>\n"
    "                               bsd (the default), posix, just-symbols, darwin\n"
    "                               or sysv\n"
    "    -B, -P, -j, -m             bsd, posix, just-symbols, darwin\n"
    "    -C, --demangle             demangle C++ names; --no-demangle undoes it\n"
    "    -g, --extern-only          only symbols that are not local\n"
    "    -u, --undefined-only       only undefined symbols\n"
    "    -U, --defined-only         only defined symbols\n"
    "    -W, --no-weak              no weak symbols\n"
    "    -n, -v, --numeric-sort     sort by address\n"
    "    --size-sort                sort by size\n"
    "    -p, --no-sort              in the file's order\n"
    "    -r, --reverse-sort         sort backwards\n"
    "    -S, --print-size           print each symbol's size\n"
    "    -t <radix>, --radix=<radix>\n"
    "                               o, d or x (the default)\n"
    "    -M, --print-armap          print an archive's symbol table\n"
    "    --export-symbols           every file's defined external names, once each\n"
    "    --quiet                    no note of a file with no symbols\n"
    "A file is a program, an object or an archive; - is standard input.\n";

// Everything the front end holds, off the coroutine frame.
struct Front {
    NmArgs args;
    Nm nm;
    Diag diag;
    Vec<String> kept; // every file, for --export-symbols
    String in;        // standard input, read whole
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
    if (s.nm.out.oom || s.diag.text.oom) {
        s.nm.out.clear();
        s.diag.text.clear();
        s.diag.error("out of memory");
    }
    co_await say(SYS_STDOUT, s.nm.out.str());
    co_await say(SYS_STDERR, s.diag.text.str());
    s.nm.out.clear();
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

Task<void> nm_one(Front &s, Str path)
{
    bool in          = path == "-";
    Result<String> r = in ? co_await read_stdin() : co_await slurp(path);
    if (r.is_err()) {
        failed(s, path, r.error());
        co_return;
    }
    // --export-symbols keeps every file, as the names it prints are views.
    String *f = &s.in;
    if (s.args.nm.export_symbols) {
        if (!s.kept.push(move(r.value()))) {
            s.diag.error("out of memory");
            co_return;
        }
        f = &s.kept.back();
    } else {
        s.in = move(r.value());
    }
    nm_file(s.nm, in ? "<stdin>"_s : path,
            Bytes(reinterpret_cast<const u8 *>(f->data()), f->size()), s.diag);
}

Task<i32> run(Front &s, Args args)
{
    s.diag.tool  = "nm";
    s.diag.limit = 0;
    Vec<Str> words;
    bool ok = true;
    for (usize i = 1; ok && i < args.size(); i++)
        if (!words.push(args[i])) {
            s.diag.error("out of memory");
            ok = false;
        }
    // A bad --format, --radix or -X is reported, and the files are read anyway.
    if (ok && parse_args(words, s.args, s.diag)) {
        if (s.args.help) {
            co_await say(SYS_STDOUT, USAGE);
            co_return 0;
        }
        if (s.args.version) {
            co_await say(SYS_STDOUT, "nm (wasm), compatible with GNU nm\n");
            co_return 0;
        }
        s.nm.c        = s.args.nm;
        s.nm.multiple = s.args.inputs.size() > 1;
        for (Str path : s.args.inputs) {
            co_await nm_one(s, path);
            if (s.cancelled)
                break;
            co_await flush(s);
        }
        if (!s.cancelled)
            nm_exports(s.nm);
    }
    co_await flush(s);
    if (s.cancelled)
        co_return 130;
    co_return s.diag.errors ? 1 : 0;
}

} // namespace

Task<i32> proc_main(Args args)
{
    if (args.size() == 1 || help_asked(args))
        co_return co_await usage_asked(USAGE);
    Front *s = heap_new<Front>();
    if (!s)
        co_return 1;
    i32 status = co_await run(*s, args);
    heap_delete(s);
    co_return status;
}
