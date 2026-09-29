// The Braam front end: reads what the command line names, hands the bytes to
// the core, and writes what comes back. Only this file awaits.
#include "demangle/demangle.h"
#include "driver.h"
#include "dump.h"
#include "files.h"
#include "fs/path.h"
#include "gc.h"
#include "kernel/alloc.h"
#include "proc/io.h"
#include "symtab.h"
#include "writer.h"

namespace {

// Everything the front end holds, off the coroutine frame.
struct Front {
    Vec<String> texts; // response files and inputs, kept alive
    Vec<Str> words;
    Vec<Source> inputs;
    Config cfg;
    Diag diag;
    Out out;
    String path;   // where the last input was read from
    String libdir; // the package's lib/, or empty
    Vec<u8> image; // the output module
    Out map;       // -Map
    bool cancelled = false;
};

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
    if (why == Error::Cancelled) {
        s.cancelled = true;
        return;
    }
    Out m;
    m.put("cannot open ").put(path).put(": ").put(error_name(why));
    s.diag.error(m.str());
}

// Stderr, then the status: 130 for ^C, 1 for any other failure.
Task<i32> finish(Front &s, bool ok)
{
    co_await say(SYS_STDERR, s.diag.text.str());
    if (s.cancelled)
        co_return 130;
    co_return ok && !s.diag.failed() ? 0 : 1;
}

// The package's lib/, beside the bin/ that /pkg/bin/ld leads to, or in the
// first wasm package in the store. It holds crt.o and libw.a.
Task<void> find_libdir(Front &s)
{
    Result<String> link = co_await read_link("/pkg/bin/ld");
    if (link.is_ok()) {
        s.libdir.assign(path_dirname(path_dirname(link.value().str())));
        s.libdir.append("/lib");
        co_return;
    }
    Result<Vec<DirEntry>> ents = co_await list_dir("/pkg/store");
    if (ents.is_ok())
        for (const DirEntry &e : ents.value())
            if (e.name.str().starts_with("wasm-")) {
                s.libdir.assign("/pkg/store/");
                s.libdir.append(e.name.str());
                s.libdir.append("/lib");
                co_return;
            }
}

// Reads dir/prefix+name+suffix into s.path's file; NotFound if it is not there.
Task<Result<String>> read_in(Front &s, Str dir, Str prefix, Str name, Str suffix)
{
    s.path.assign(dir);
    s.path.append("/");
    s.path.append(prefix);
    s.path.append(name);
    s.path.append(suffix);
    co_return co_await slurp(s.path.str());
}

// An input, which s.path then names. A library is looked for in each -L
// directory, then in the package's lib/; an object named without a
// directory, in the working directory, then in lib/.
Task<Result<String>> read_input(Front &s, const InputArg &a, Error &why)
{
    if (!a.lib) {
        s.path.assign(a.name);
        Result<String> r = co_await slurp(a.name);
        if (r.is_err() && r.error() == Error::NotFound && !s.libdir.empty() &&
            a.name.find('/') == Str::npos)
            r = co_await read_in(s, s.libdir.str(), "", a.name, "");
        if (r.is_err()) {
            s.path.assign(a.name);
            why = r.error();
        }
        co_return r;
    }
    for (usize i = 0; i <= s.cfg.lib_dirs.size(); i++) {
        Str dir = i < s.cfg.lib_dirs.size() ? s.cfg.lib_dirs[i] : s.libdir.str();
        if (dir.empty())
            continue;
        Result<String> r = co_await read_in(s, dir, "lib", a.name, ".a");
        if (r.is_ok() || r.error() != Error::NotFound) {
            if (r.is_err())
                why = r.error();
            co_return r;
        }
    }
    why = Error::NotFound;
    co_return Err(Error::NotFound);
}

// --dump-demangle: each line of `text` as wasm-ld would show the name.
bool demangle_lines(Str text, Out &out)
{
    String tmp;
    usize at = 0;
    while (at < text.size()) {
        usize end = at;
        while (end < text.size() && text[end] != '\n')
            end++;
        demangle(text.substr(at, end - at), tmp);
        out.put(tmp.str()).put('\n');
        at = end + 1;
    }
    return !out.oom;
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
        bool ok = s.cfg.dump_demangle ? demangle_lines(file.str(), s.out)
                                      : dump_file(a.name, bytes, s.out, err);
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
            if (why == Error::Cancelled) {
                s.cancelled = true;
                co_return co_await finish(s, false);
            }
            if (a.lib) {
                Out m;
                m.put("unable to find library -l").put(a.name);
                s.diag.error(m.str());
            } else {
                cannot_open(s, a.name, why);
            }
            continue;
        }
        // A library, or an object found in lib/, is named by its path.
        Str name = a.name;
        if (s.path.str() != a.name) {
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
    ok = ok && write_module(*l, s.image);
    if (ok && !s.cfg.map_file.empty())
        write_map(*l, s.map);
    if (ok && s.cfg.dump_symtab)
        dump_symtab(*l, l->out);
    if (l->out.oom || l->why.oom)
        s.diag.error("out of memory");
    Result<void> said = co_await say(SYS_STDOUT, l->out.str());
    if (said.is_err() && said.error() == Error::Cancelled)
        s.cancelled = true;
    if (!s.cfg.why_extract.empty()) {
        l->why.s.insert(0, "reference\textracted\tsymbol\n");
        Result<void> w = s.cfg.why_extract == "-" ? co_await say(SYS_STDOUT, l->why.str())
                                                  : co_await spill(s.cfg.why_extract, l->why.str());
        if (w.is_err())
            cannot_open(s, s.cfg.why_extract, w.error());
    }
    heap_delete(l);
    // Only a link that succeeded leaves a file.
    if (ok && !s.cancelled) {
        Str to = s.cfg.output.empty() ? "a.out"_s : s.cfg.output;
        Str bytes(reinterpret_cast<const char *>(s.image.data()), s.image.size());
        Result<void> w = co_await spill(to, bytes);
        if (w.is_err())
            cannot_open(s, to, w.error());
        if (!s.cfg.map_file.empty()) {
            if (s.map.oom)
                s.diag.error("out of memory");
            else if (Result<void> mw = co_await spill(s.cfg.map_file, s.map.str()); mw.is_err())
                cannot_open(s, s.cfg.map_file, mw.error());
        }
    }
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
    co_await find_libdir(s);
    if (s.cfg.dump || s.cfg.dump_demangle)
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
