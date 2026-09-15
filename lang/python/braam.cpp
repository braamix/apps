// The Braam platform: proc_main, and every co_await in the program. The
// interpreter under it stays plain C++ and hands the driver what to do --
// simbesm's cpu_burst() shape, and for the same reason: a co_await is a call
// and not a tail call.
//
// Phase 0 is the command line and the banner. TODO.md says what follows.
#include "compile.h"
#include "err.h"
#include "kernel/args.h"
#include "kernel/fmt.h"
#include "lex.h"
#include "parse.h"
#include "proc/io.h"
#include "proc/opt.h"
#include "proc/rt.h"
#include "proc/usage.h"
#include "selftest.h"

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
    "own virtual machine. Nothing runs yet -- see TODO.md.\n";

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

} // namespace

Task<i32> proc_main(Args args)
{
    if (help_asked(args))
        co_return co_await usage_asked(USAGE);
    // OptParse has no long options, so the two long ones are answered first.
    for (usize i = 1; i < args.size(); i++) {
        if (args[i] == "--version")
            co_return co_await banner();
        if (args[i] == "--dump-tokens") {
            Input in(Args{ args.v.subspan(i + 1) }, SYS_STDIN, WHO);
            Result<String> src = co_await in.read();
            if (src.is_err())
                co_return 1;
            String out;
            bool ok = lex_dump(src.value().str(), out);
            co_await write_all(SYS_STDOUT, out.str());
            if (!ok)
                co_await write_all(SYS_STDERR, where().str());
            co_return ok ? 0 : 1;
        }
        if (args[i] == "--dump-ast") {
            Input in(Args{ args.v.subspan(i + 1) }, SYS_STDIN, WHO);
            Result<String> src = co_await in.read();
            if (src.is_err())
                co_return 1;
            String out;
            bool ok = ast_dump(src.value().str(), out);
            co_await write_all(SYS_STDOUT, out.str());
            if (!ok)
                co_await write_all(SYS_STDERR, where().str());
            co_return ok ? 0 : 1;
        }
        if (args[i] == "--dis") {
            Args tail{ args.v.subspan(i + 1) };
            Input in(tail, SYS_STDIN, WHO);
            Result<String> src = co_await in.read();
            if (src.is_err())
                co_return 1;
            String out;
            bool ok = py_dis(src.value().str(), tail.size() ? tail[0] : Str("<stdin>"), out);
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

    // Phase 0: the compiler, the VM and the driver loop belong here.
    if (!job.command.empty())
        co_return co_await complain("no interpreter yet, so -c does nothing", Str());
    if (!job.file.empty())
        co_return co_await complain("no interpreter yet, cannot run", job.file);

    // No file and no -c: where the REPL will start.
    co_return co_await banner();
}
