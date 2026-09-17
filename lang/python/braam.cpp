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
#include "kernel/alloc.h"
#include "kernel/args.h"
#include "kernel/fmt.h"
#include "lex.h"
#include "module.h"
#include "parse.h"
#include "posix.h"
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
    "own virtual machine. There is no library, no event loop and no REPL yet\n"
    "-- see TODO.md.\n";

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

// ------------------------------------------------------------ system calls
//
// One coroutine per family, so that no frame carries every kind of Result
// at once; each stays well under the 512 bytes a frame is allowed.

void fail(SysAns &a, Error e)
{
    a.ok  = false;
    a.err = e;
}

void set_info(SysAns &a, const FileInfo &st)
{
    a.ok    = true;
    a.kind  = st.kind;
    a.size  = st.size;
    a.mtime = st.mtime;
}

// Whatever answers a Result<void>.
void done_void(SysAns &a, const Result<void> &r)
{
    if (r.is_err())
        fail(a, r.error());
    else
        a.ok = true;
}

// The files O_TMPFILE made, each removed when its descriptor closes.
struct Hidden {
    i32 fd;
    String path;
};
Vec<Hidden> *hidden;

Task<Result<i32>> open_hidden(Str dir, u32 flags)
{
    if (!hidden)
        hidden = heap_new<Vec<Hidden>>();
    if (!hidden)
        co_return Err(Error::NoMemory);
    flags = (flags & ~(SYS_O_HIDDEN | SYS_O_TRUNC)) | SYS_O_CREATE | SYS_O_EXCL;
    for (u32 tries = 0;; tries++) {
        Buf<32> leaf;
        leaf.put(".pytmp-").put(proc_pid()).put('-').put_hex(proc_random());
        Hidden h{ -1, String() };
        if (path_join(dir, leaf.str(), h.path).is_err())
            co_return Err(Error::NoMemory);
        Result<i32> r = Err(Error::NoMemory);
        if (Task<Result<i32>> t = open_at(h.path.str(), flags))
            r = co_await t;
        if (r.is_err() && r.error() == Error::Exists && tries < 16)
            continue;
        if (r.is_err())
            co_return r;
        h.fd = r.value();
        if (!hidden->push(static_cast<Hidden &&>(h))) {
            co_await sys_call(Sys::Close, u32(r.value()));
            co_return Err(Error::NoMemory);
        }
        co_return r;
    }
}

// The path to remove once `fd` is closed; empty when it is not one of them.
String hidden_take(i32 fd)
{
    String out;
    for (usize i = 0; hidden && i < hidden->size(); i++)
        if ((*hidden)[i].fd == fd) {
            out = static_cast<String &&>((*hidden)[i].path);
            (*hidden)[i] = static_cast<Hidden &&>((*hidden)[hidden->size() - 1]);
            hidden->pop();
            break;
        }
    return out;
}

Task<void> hidden_sweep()
{
    while (hidden && !hidden->empty()) {
        String p = hidden_take((*hidden)[0].fd);
        if (Task<Result<void>> t = remove_path(p.str(), false))
            co_await t;
    }
}

// Every Task below is null when there was no memory for its frame.
Task<void> sys_stream(const SysReq &q, SysAns &a)
{
    switch (q.op) {
    case SysOp::Read: {
        Result<String> r = Err(Error::NoMemory);
        if (Task<Result<String>> t = read_some(u32(q.fd), q.max ? q.max : SYS_READ_MAX))
            r = co_await t;
        if (r.is_ok()) {
            a.ok   = true;
            a.data = static_cast<String &&>(r.value());
        } else if (r.error() == Error::Closed) {
            a.ok = true; // the end of input is an empty read
        } else {
            fail(a, r.error());
        }
        break;
    }
    case SysOp::Write: {
        Result<void> r = Err(Error::NoMemory);
        if (Task<Result<void>> t = write_all(u32(q.fd), q.data))
            r = co_await t;
        done_void(a, r);
        a.n = i64(q.data.size());
        break;
    }
    case SysOp::Open: {
        Result<i32> r = Err(Error::NoMemory);
        if (q.flags & SYS_O_HIDDEN) {
            if (Task<Result<i32>> t = open_hidden(q.path, q.flags))
                r = co_await t;
        } else if (Task<Result<i32>> t = open_at(q.path, q.flags)) {
            r = co_await t;
        }
        if (r.is_ok()) {
            a.ok = true;
            a.n  = r.value();
        } else {
            fail(a, r.error());
        }
        break;
    }
    case SysOp::Close: {
        // close_fd says nothing; a descriptor that was not open is an error.
        Result<SysReply> r = co_await sys_call(Sys::Close, u32(q.fd));
        if (r.is_ok())
            a.ok = true;
        else
            fail(a, r.error());
        if (r.is_ok() && hidden) {
            String p = hidden_take(q.fd);
            if (!p.empty())
                if (Task<Result<void>> t = remove_path(p.str(), false))
                    co_await t;
        }
        break;
    }
    case SysOp::Dup: {
        Result<u32> r = Err(Error::NoMemory);
        if (Task<Result<u32>> t = dup_fd(u32(q.fd)))
            r = co_await t;
        if (r.is_ok()) {
            a.ok = true;
            a.n  = r.value();
        } else {
            fail(a, r.error());
        }
        break;
    }
    case SysOp::Pipe: {
        Result<Piped> r = Err(Error::NoMemory);
        if (Task<Result<Piped>> t = make_pipe())
            r = co_await t;
        if (r.is_ok()) {
            a.ok  = true;
            a.n   = r.value().r;
            a.off = r.value().w;
        } else {
            fail(a, r.error());
        }
        break;
    }
    default:
        fail(a, Error::Unsupported);
    }
}

Task<void> sys_file(const SysReq &q, SysAns &a)
{
    switch (q.op) {
    case SysOp::Seek: {
        Result<u64> r = Err(Error::NoMemory);
        if (Task<Result<u64>> t = seek_fd(u32(q.fd), q.off, q.whence))
            r = co_await t;
        if (r.is_ok()) {
            a.ok = true;
            a.n  = i64(r.value());
        } else {
            fail(a, r.error());
        }
        break;
    }
    case SysOp::Truncate: {
        Result<void> r = Err(Error::NoMemory);
        if (Task<Result<void>> t = truncate_fd(u32(q.fd), u64(q.off)))
            r = co_await t;
        done_void(a, r);
        break;
    }
    case SysOp::Stat:
    case SysOp::FStat: {
        Result<FileInfo> r = Err(Error::NoMemory);
        if (q.op == SysOp::Stat) {
            if (Task<Result<FileInfo>> t = stat_of(q.path, q.follow))
                r = co_await t;
        } else if (Task<Result<FileInfo>> t = stat_fd(u32(q.fd))) {
            r = co_await t;
        }
        if (r.is_ok())
            set_info(a, r.value());
        else
            fail(a, r.error());
        break;
    }
    case SysOp::Tty: {
        Result<TtyInfo> r = Err(Error::NoMemory);
        if (Task<Result<TtyInfo>> t = tty_of(u32(q.fd)))
            r = co_await t;
        if (r.is_ok()) {
            a.ok   = true;
            a.n    = r.value().console ? 1 : 0;
            a.kind = r.value().at.cols;
            a.size = r.value().at.rows;
        } else {
            fail(a, r.error());
        }
        break;
    }
    case SysOp::SigCatch: {
        Result<void> r = Err(Error::NoMemory);
        if (Task<Result<void>> t = sig_catch(u32(q.fd), q.flags != 0))
            r = co_await t;
        done_void(a, r);
        break;
    }
    case SysOp::Kill: {
        Result<void> r = Err(Error::NoMemory);
        if (Task<Result<void>> t = kill_child(u32(q.fd), q.flags))
            r = co_await t;
        done_void(a, r);
        break;
    }
    default:
        fail(a, Error::Unsupported);
    }
}

Task<void> sys_list(const SysReq &q, SysAns &a)
{
    Result<Vec<DirEntry>> r = Err(Error::NoMemory);
    if (Task<Result<Vec<DirEntry>>> t = list_dir(q.path))
        r = co_await t;
    if (r.is_err()) {
        fail(a, r.error());
        co_return;
    }
    a.ok = true;
    for (const DirEntry &e : r.value()) {
        SysEnt s;
        if (!s.name.assign(e.name.str())) {
            fail(a, Error::NoMemory);
            co_return;
        }
        s.kind  = e.kind;
        s.size  = e.size;
        s.mtime = e.mtime;
        if (!a.ents.push(static_cast<SysEnt &&>(s))) {
            fail(a, Error::NoMemory);
            co_return;
        }
    }
}

Task<void> sys_names(const SysReq &q, SysAns &a)
{
    Result<void> r = Err(Error::NoMemory);
    switch (q.op) {
    case SysOp::MkDir:
        if (Task<Result<void>> t = make_dir(q.path))
            r = co_await t;
        break;
    case SysOp::Remove:
        if (Task<Result<void>> t = remove_path(q.path, q.flags & 1))
            r = co_await t;
        break;
    case SysOp::Rename:
        if (Task<Result<void>> t = rename_path(q.path, q.path2))
            r = co_await t;
        break;
    case SysOp::Symlink:
        if (Task<Result<void>> t = make_link(q.data, q.path))
            r = co_await t;
        break;
    case SysOp::Touch:
        if (Task<Result<void>> t = touch_path(q.path))
            r = co_await t;
        break;
    default:
        r = Err(Error::Unsupported);
    }
    done_void(a, r);
}

Task<void> sys_text(const SysReq &q, SysAns &a)
{
    Result<String> r = Err(Error::NoMemory);
    if (q.op == SysOp::ReadLink) {
        if (Task<Result<String>> t = read_link(q.path))
            r = co_await t;
    } else if (q.op == SysOp::Cwd) {
        if (Task<Result<String>> t = cwd_get())
            r = co_await t;
    } else if (Task<Result<String>> t = cwd_set(q.path)) {
        r = co_await t;
    }
    if (r.is_ok()) {
        a.ok   = true;
        a.data = static_cast<String &&>(r.value());
    } else {
        fail(a, r.error());
    }
}

// The answer lives here rather than in a frame: a listing can be long.
SysAns *answer;

Task<void> perform(const SysReq &q)
{
    if (!answer)
        answer = heap_new<SysAns>();
    if (!answer)
        co_return;
    SysAns &a = *answer;
    a         = SysAns{};
    Task<void> t;
    switch (q.op) {
    case SysOp::Read:
    case SysOp::Write:
    case SysOp::Open:
    case SysOp::Close:
    case SysOp::Dup:
    case SysOp::Pipe:
        t = sys_stream(q, a);
        break;
    case SysOp::List:
        t = sys_list(q, a);
        break;
    case SysOp::MkDir:
    case SysOp::Remove:
    case SysOp::Rename:
    case SysOp::Symlink:
    case SysOp::Touch:
        t = sys_names(q, a);
        break;
    case SysOp::ReadLink:
    case SysOp::Cwd:
    case SysOp::Chdir:
        t = sys_text(q, a);
        break;
    default:
        t = sys_file(q, a);
    }
    if (t)
        co_await t;
    else
        fail(a, Error::NoMemory);
}

// What arrived while the driver was parked, for the VM to deliver.
void take_signals()
{
    if (sig_take(SIG_INT))
        vm_interrupt();
    if (sig_take(SIG_TERM))
        vm_signal(SIG_TERM);
    if (sig_take(SIG_WINCH))
        vm_signal(SIG_WINCH);
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
    if (!vm_start(code.v, argv, script.empty() ? Str() : name)) {
        co_await write_all(SYS_STDERR, where().str());
        co_return 1;
    }
    // Two things the interpreter cannot ask for itself, because both are
    // asynchronous syscalls and nothing under vm_burst awaits: whether the
    // three descriptors are the terminal, and what day it is. Read once here
    // and handed over; time.time() counts on from this reading with Sys::Now,
    // which is monotonic and cannot name a day of its own.
    bool tty[3] = { false, false, false };
    for (u32 i = 0; i < 3; i++) {
        Result<TtyInfo> t = Err(Error::NoMemory);
        if (Task<Result<TtyInfo>> q = tty_of(i))
            t = co_await q;
        tty[i] = t.is_ok() && t.value().console;
    }
    sys_set_tty(tty[0], tty[1], tty[2]);
    Result<Clock> clock = Err(Error::NoMemory);
    if (Task<Result<Clock>> q = clock_now())
        clock = co_await q;
    if (clock.is_ok())
        time_set_clock(clock.value().epoch_ms, clock.value().tz_min, proc_now());
    // sys.path: the directory the program came from, then the shipped library.
    // A `-c` or a pipe has no directory of its own, and gets the cwd.
    Result<String> cwd = Err(Error::NoMemory);
    if (Task<Result<String>> q = cwd_get())
        cwd = co_await q;
    sys_set_cwd(cwd.is_ok() ? cwd.value().str() : Str("/"));
    String lib;
    co_await find_library(lib);
    sys_set_path(script.empty() ? Str(".") : path_dirname(script), lib.str());

    for (;;) {
        Req r = vm_burst();
        if (r.kind == ReqKind::Exit) {
            co_await hidden_sweep();
            co_return r.status;
        }
        if (r.kind == ReqKind::Tick) {
            // The burst is up. Parking is the only thing that lets a signal
            // in, and a zero sleep is the cheapest park there is.
            if (Task<Result<void>> t = sleep_for(0))
                co_await t;
            take_signals();
            continue;
        }
        if (r.kind == ReqKind::Sleep) {
            // time.sleep. A ^C abandons it, which is what makes a long sleep
            // interruptible; the raise happens at the next instruction.
            if (Task<Result<void>> t = sleep_for(r.ms))
                co_await t;
            take_signals();
            vm_sleep_done();
            continue;
        }
        if (r.kind == ReqKind::Sys) {
            // io and os. A ^C abandons a read, which answers Err(Intr); the
            // step that asked takes the interrupt from there.
            if (Task<void> t = perform(*r.sys))
                co_await t;
            take_signals();
            if (answer) {
                vm_sys_done(*answer);
            } else {
                SysAns none;
                none.err = Error::NoMemory;
                vm_sys_done(none);
            }
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
