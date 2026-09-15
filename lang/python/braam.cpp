// The Braam platform: proc_main, and every co_await in the program. The
// interpreter under it stays plain C++ and hands the driver what to do --
// simbesm's cpu_burst() shape, and for the same reason: a co_await is a call
// and not a tail call.
//
// The command line, the three ways a program arrives, and the loop that
// performs what vm_burst asks for. TODO.md says what is still missing.
#include "compile.h"
#include "err.h"
#include "gc.h"
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
    "own virtual machine. Classes, exceptions and generators are not here\n"
    "yet -- see TODO.md.\n";

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

// The whole of the platform half: compile, then hand the VM to the driver
// loop below. Only this task awaits; the interpreter under it is plain C++
// and says what it wants done.
Task<i32> interpret(Str source, Str name, Args argv)
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

    for (;;) {
        Req r = vm_burst();
        if (r.kind == ReqKind::Exit)
            co_return r.status;
        vm_write_done(!(co_await write_all(r.fd, r.data)).is_err());
    }
}

} // namespace

Task<i32> proc_main(Args args)
{
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

    co_return co_await interpret(source.str(), name, rest);
}
