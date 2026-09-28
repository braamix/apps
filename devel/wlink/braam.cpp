// The Braam front end: reads what the command line names, hands the bytes to
// the core, and writes what comes back. Only this file awaits.
//
// Linking stops after layout for now: nothing is written to -o.
#include "driver.h"
#include "dump.h"
#include "gc.h"
#include "kernel/alloc.h"
#include "proc/io.h"
#include "symtab.h"

namespace {

// Everything the front end holds, off the coroutine frame.
struct Front {
    Vec<String> texts; // response files and inputs, kept alive
    Vec<Str> words;
    Vec<Source> inputs;
    Config cfg;
    Diag diag;
    Out out;
    String path;
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

// Writes `s` to a file, replacing it.
Task<Result<void>> spill(Str path, Str s)
{
    Task<Result<i32>> o = open_at(path, SYS_O_WRITE | SYS_O_CREATE | SYS_O_TRUNC);
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

void cannot_open(Front &s, Str path, Error why)
{
    Out m;
    m.put("cannot open ").put(path).put(": ").put(error_name(why));
    s.diag.error(m.str());
}

// Stderr, then the status: 130 for ^C, 1 for any other failure.
Task<i32> finish(Front &s, bool ok)
{
    co_await say(SYS_STDERR, s.diag.text.str());
    co_return ok && !s.diag.failed() ? 0 : 1;
}

Task<Result<String>> read_input(Front &s, const InputArg &a, Error &why)
{
    if (!a.lib) {
        Result<String> r = co_await slurp(a.name);
        if (r.is_err())
            why = r.error();
        co_return r;
    }
    for (Str dir : s.cfg.lib_dirs) {
        s.path.assign(dir);
        s.path.append("/lib");
        s.path.append(a.name);
        s.path.append(".a");
        Result<String> r = co_await slurp(s.path.str());
        if (r.is_ok() || r.error() != Error::NotFound) {
            if (r.is_err())
                why = r.error();
            co_return r;
        }
    }
    why = Error::NotFound;
    co_return Err(Error::NotFound);
}

Task<i32> dump(Front &s)
{
    for (const InputArg &a : s.cfg.inputs) {
        Error why        = Error::Io;
        Result<String> r = co_await read_input(s, a, why);
        if (r.is_err()) {
            cannot_open(s, a.name, why);
            co_return co_await finish(s, false);
        }
        String &file = r.value();
        Bytes bytes(reinterpret_cast<const u8 *>(file.data()), file.size());
        Out err;
        s.out.clear();
        bool ok = dump_file(a.name, bytes, s.out, err);
        if (s.out.oom)
            s.diag.error("out of memory");
        Result<void> w = co_await say(SYS_STDOUT, s.out.str());
        if (w.is_err())
            co_return w.error() == Error::Cancelled ? 130 : 1;
        if (!ok) {
            s.diag.error(err.str());
            co_return co_await finish(s, false);
        }
    }
    co_return co_await finish(s, true);
}

Task<i32> link(Front &s)
{
    for (const InputArg &a : s.cfg.inputs) {
        Error why        = Error::Io;
        Result<String> r = co_await read_input(s, a, why);
        if (r.is_err()) {
            if (a.lib) {
                Out m;
                m.put("unable to find library -l").put(a.name);
                s.diag.error(m.str());
            } else {
                cannot_open(s, a.name, why);
            }
            continue;
        }
        // A library is named by the path it was found at.
        Str name = a.name;
        if (a.lib) {
            String p;
            if (!p.append(s.path.str()) || !s.texts.push(move(p)))
                s.diag.error("out of memory");
            name = s.texts.back().str();
        }
        if (!s.texts.push(move(r.value())))
            s.diag.error("out of memory");
        const String &t = s.texts.back();
        s.inputs.push(Source{ name, Bytes(reinterpret_cast<const u8 *>(t.data()), t.size()) });
    }
    if (s.diag.failed())
        co_return co_await finish(s, false);

    Linker *l = heap_new<Linker>(s.cfg, s.diag);
    if (!l) {
        s.diag.error("out of memory");
        co_return co_await finish(s, false);
    }
    bool ok = resolve(*l, s.inputs) && mark_live(*l);
    if (ok && s.cfg.print_gc_sections)
        print_gc_sections(*l, l->out);
    ok = ok && check_undefined(*l) && layout(*l);
    if (ok && s.cfg.dump_layout)
        dump_layout(*l, l->out);
    if (ok && s.cfg.dump_symtab)
        dump_symtab(*l, l->out);
    if (l->out.oom || l->why.oom)
        s.diag.error("out of memory");
    co_await say(SYS_STDOUT, l->out.str());
    if (!s.cfg.why_extract.empty()) {
        l->why.s.insert(0, "reference\textracted\tsymbol\n");
        Result<void> w = s.cfg.why_extract == "-" ? co_await say(SYS_STDOUT, l->why.str())
                                                  : co_await spill(s.cfg.why_extract, l->why.str());
        if (w.is_err())
            cannot_open(s, s.cfg.why_extract, w.error());
    }
    heap_delete(l);
    co_return co_await finish(s, ok);
}

Task<i32> run(Front &s, Args args)
{
    for (usize i = 1; i < args.size(); i++) {
        Str a = args[i];
        if (!a.starts_with("@")) {
            s.words.push(a);
            continue;
        }
        Result<String> r = co_await slurp(a.substr(1));
        if (r.is_err()) {
            cannot_open(s, a.substr(1), r.error());
            co_return co_await finish(s, false);
        }
        if (!s.texts.push(move(r.value())) || !split(s.texts.back().str(), s.words)) {
            s.diag.error("out of memory");
            co_return co_await finish(s, false);
        }
    }
    if (!parse_args(s.words, s.cfg, s.diag))
        co_return co_await finish(s, false);
    if (s.cfg.inputs.empty()) {
        s.diag.error("no input files");
        co_return co_await finish(s, false);
    }
    if (s.cfg.dump)
        co_return co_await dump(s);
    co_return co_await link(s);
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
