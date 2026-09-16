// The Braam platform: proc_main, and every co_await in the program. The
// interpreter under it stays plain C++ and hands the driver what to do --
// simbesm's cpu_burst() shape, and for the same reason: a co_await is a call
// and not a tail call.
//
// The command line, the three ways a program arrives, and the loop that
// performs what vm_burst asks for. TODO.md says what is still missing.
#include "compile.h"
#include "err.h"
#include "fs/path.h"
#include "gc.h"
#include "import.h"
#include "kernel/args.h"
#include "kernel/fmt.h"
#include "lex.h"
#include "parse.h"
#include "proc/io.h"
#include "proc/opt.h"
#include "proc/rt.h"
#include "proc/usage.h"
#include "selftest.h"
#include "vm.h"

namespace {

constexpr Str WHO = "python";

// The number is this implementation's; the language it aims at is 3.
constexpr Str VERSION = "Python 0.1 on Braam";

constexpr Str USAGE =
    "Usage:\n"
    "    python <file> [arg]...   run the program in <file>\n"
    "    python -c <cmd> [arg]... run <cmd>\n"
    "    python - [arg]...        read the program from stdin\n"
    "    python -V                print the version\n"
    "    python --dump-tokens <f> print the token stream of <f>\n"
    "    python --dump-ast <f>    print the parse tree of <f>\n"
    "    python --dis <f>         print the bytecode of <f>\n"
    "\n"
    "Python 3, written for Braam: its own compiler, its own bytecode and its\n"
    "own virtual machine. There is no library and no REPL yet, and `async` is\n"
    "refused -- see TODO.md.\n";

constexpr Opts SPEC = { "V", "c" };

// What the command line settles.
struct Job {
    Str command; // -c, and then source is the command itself
    Str file;    // a path, or "-" for stdin
    bool version = false;
};

// The pending error, with the place it was found.
Buf<192> where()
{
    Buf<192> b;
    b.put(WHO).put(": ").put(u64(err_line())).put(':').put(u64(err_col()));
    b.put(": ").put(err_kind()).put(": ").put(err_message()).put('\n');
    return b;
}

Task<i32> banner()
{
    Buf<64> b;
    b.put(VERSION).put('\n');
    co_return (co_await write_all(SYS_STDOUT, b.str())).is_err() ? 1 : 0;
}

Task<i32> complain(Str what, Str detail)
{
    Buf<128> b;
    b.put(WHO).put(": ").put(what);
    if (!detail.empty())
        b.put(": ").put(detail);
    b.put('\n');
    co_await write_all(SYS_STDERR, b.str());
    co_return 1;
}

// A whole source. Input::read hands over one chunk at a time, and a program
// is not one chunk.
Task<bool> slurp(Args paths, String &out)
{
    Input in(paths, SYS_STDIN, WHO);
    for (;;) {
        Result<String> got = co_await in.read();
        if (got.is_err())
            co_return got.error() == Error::Closed;
        if (!out.append(got.value().str()))
            co_return false;
    }
}

// Is `root`/share/lib a directory? mbasic's epath.cpp resolves its examples
// the same way, and for the same reason: the store path carries a version the
// binary does not know.
Task<bool> holds_library(Str root, String &out)
{
    if (!out.assign(root) || !out.append("/share/lib"))
        co_return false;
    Result<FileInfo> st = Err(Error::NoMemory);
    if (Task<Result<FileInfo>> t = stat_of(out.str()))
        st = co_await t;
    if (st.is_ok() && st.value().kind == SYS_KIND_DIR)
        co_return true;
    out.clear();
    co_return false;
}

// Where the shipped library is. pkg writes /pkg/bin/python as a symlink into
// the store, and readlink does not follow the leaf, so one syscall recovers
// the prefix; failing that, the store is scanned for the one directory this
// package's name is a prefix of.
Task<void> find_library(String &out)
{
    Result<String> link = Err(Error::NoMemory);
    if (Task<Result<String>> t = read_link("/pkg/bin/python"))
        link = co_await t;
    if (link.is_ok()) {
        Str dir = path_dirname(link.value().str()); // .../bin
        if (Task<bool> t = holds_library(path_dirname(dir), out))
            if (co_await t)
                co_return;
    }

    Result<Vec<DirEntry>> ents = Err(Error::NoMemory);
    if (Task<Result<Vec<DirEntry>>> t = list_dir("/pkg/store"))
        ents = co_await t;
    if (ents.is_err())
        co_return;
    String cand;
    for (const DirEntry &e : ents.value()) {
        if (!e.name.str().starts_with("python-"))
            continue;
        if (path_join("/pkg/store", e.name.str(), cand).is_err())
            continue;
        if (Task<bool> t = holds_library(cand.str(), out))
            if (co_await t)
                co_return;
    }
}

// The whole of the platform half: compile, then hand the VM to the driver
// loop below. Only this task awaits; the interpreter under it is plain C++
// and says what it wants done.
Task<i32> interpret(Str source, Str name, Args argv, Str script)
{
    // Collecting at every allocation turns a missing Root into a wrong answer
    // rather than a rare crash. It is slow, so a program asks for it by name.
    if (!proc_env("PY_GC_STRESS").empty())
        gc_stress(true);

    Ast ast;
    if (!ast.parse(source)) {
        co_await write_all(SYS_STDERR, where().str());
        co_return 1;
    }
    Root code{ py_compile(ast, name) };
    if (code.v.is_nil()) {
        co_await write_all(SYS_STDERR, where().str());
        co_return 1;
    }
    if (!vm_start(code.v, argv)) {
        co_await write_all(SYS_STDERR, where().str());
        co_return 1;
    }
    // sys.path: the directory the program came from, then the shipped library.
    // A `-c` or a pipe has no directory of its own, and gets the cwd.
    String lib;
    co_await find_library(lib);
    sys_set_path(script.empty() ? Str(".") : path_dirname(script), lib.str());

    for (;;) {
        Req r = vm_burst();
        if (r.kind == ReqKind::Exit)
            co_return r.status;
        if (r.kind == ReqKind::Tick) {
            // The burst is up. Parking is the only thing that lets a signal
            // in, and a zero sleep is the cheapest park there is.
            if (Task<Result<void>> t = sleep_for(0))
                co_await t;
            if (sig_take(SIG_INT))
                vm_interrupt();
            continue;
        }
        if (r.kind == ReqKind::Read) {
            // An import looking for a module. A missing name is an answer, not
            // a failure: the loader tries the next candidate.
            if (r.path.ends_with("/")) {
                // A namespace package: is this a directory?
                Result<FileInfo> st = Err(Error::NoMemory);
                if (Task<Result<FileInfo>> t = stat_of(r.path.substr(0, r.path.size() - 1)))
                    st = co_await t;
                bool dir = st.is_ok() && st.value().kind == SYS_KIND_DIR;
                vm_read_done(dir, dir, Str());
                continue;
            }
            Result<String> got = Err(Error::NoMemory);
            if (Task<Result<String>> t = read_file(r.path))
                got = co_await t;
            if (got.is_ok())
                vm_read_done(true, false, got.value().str());
            else
                vm_read_done(false, false, Str());
            continue;
        }
        // A write is not in the set a signal can abandon, so nothing here has
        // to worry about a half-written buffer.
        vm_write_done(!(co_await write_all(r.fd, r.data)).is_err());
    }
}

} // namespace

Task<i32> proc_main(Args args)
{
    // Before the first park, which is reading the program: a signal that
    // arrives before this is acted on rather than delivered, and the process
    // simply goes.
    co_await sig_catch(SIG_INT);

    if (help_asked(args))
        co_return co_await usage_asked(USAGE);
    // OptParse has no long options, so the two long ones are answered first.
    for (usize i = 1; i < args.size(); i++) {
        if (args[i] == "--version")
            co_return co_await banner();
        bool tokens = args[i] == "--dump-tokens";
        bool tree   = args[i] == "--dump-ast";
        if (tokens || tree || args[i] == "--dis") {
            Args tail{ args.v.subspan(i + 1) };
            String src;
            if (!co_await slurp(tail, src))
                co_return 1;
            Str name = tail.size() ? tail[0] : Str("<stdin>");
            String out;
            bool ok = tokens ? lex_dump(src.str(), out)
                      : tree ? ast_dump(src.str(), out)
                             : py_dis(src.str(), name, out);
            co_await write_all(SYS_STDOUT, out.str());
            if (!ok)
                co_await write_all(SYS_STDERR, where().str());
            co_return ok ? 0 : 1;
        }
        if (args[i] == "--selftest") {
            String out;
            bool good = selftest_run(out);
            co_await write_all(good ? SYS_STDOUT : SYS_STDERR, out.str());
            co_return good ? 0 : 1;
        }
    }

    Job job;
    OptParse opts(args, SPEC);
    for (Opt o;;) {
        Result<bool> more = opts.next(o);
        if (more.is_err()) {
            Buf<64> b;
            b.put(WHO).put(": ");
            if (more.error() == Error::NotFound)
                b.put("option -").put(o.name).put(" takes a value\n");
            else
                b.put("unknown option -").put(o.name).put('\n');
            co_await write_all(SYS_STDERR, b.str());
            co_return co_await usage_error(USAGE);
        }
        if (!more.value())
            break;
        if (o.name == 'V')
            job.version = true;
        else
            job.command = o.value; // -c, the only valued letter
    }

    Args rest = opts.rest();
    if (job.command.empty() && rest.size() > 0)
        job.file = rest[0];

    if (job.version)
        co_return co_await banner();

    // No file and no -c: where the REPL will start.
    if (job.command.empty() && job.file.empty())
        co_return co_await banner();

    // The source, and the name the traceback will carry. `-` is stdin, which
    // Input does not spell that way: it takes no path at all for that.
    String source;
    Str name = "<string>";
    if (job.command.empty()) {
        bool dash = job.file == "-";
        name      = dash ? Str("<stdin>") : job.file;
        if (!co_await slurp(Args{ rest.v.subspan(0, dash ? 0 : 1) }, source))
            co_return 1;
    } else if (!source.append(job.command)) {
        co_return co_await complain("out of memory", Str());
    }

    bool from_file = job.command.empty() && job.file != "-";
    co_return co_await interpret(source.str(), name, rest, from_file ? job.file : Str());
}
