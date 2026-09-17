// The Braam platform: proc_main, and every co_await in the program. The
// interpreter under it stays plain C++ and hands the driver what to do --
// simbesm's cpu_burst() shape, and for the same reason: a co_await is a call
// and not a tail call.
//
// The command line, the three ways a program arrives, and the loop that
// performs what vm_burst asks for. TODO.md says what is still missing.
#include "compile.h"
#include "edit.h"
#include "err.h"
#include "exc.h"
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
    "    python                   read commands at a prompt\n"
    "    python <file> [arg]...   run the program in <file>\n"
    "    python -c <cmd> [arg]... run <cmd>\n"
    "    python -m <mod> [arg]... run module <mod> as __main__\n"
    "    python - [arg]...        read the program from stdin\n"
    "    python -i ...            keep the prompt when the program ends\n"
    "    python -V                print the version\n"
    "    python --dump-tokens <f> print the token stream of <f>\n"
    "    python --dump-ast <f>    print the parse tree of <f>\n"
    "    python --dis <f>         print the bytecode of <f>\n"
    "\n"
    "Python 3, written for Braam: its own compiler, its own bytecode and its\n"
    "own virtual machine -- see TODO.md for what is still missing.\n";

constexpr Opts SPEC = { "Vi", "cm" };

// What the command line settles.
struct Job {
    Str command; // -c, and then source is the command itself
    Str module;  // -m, and then runpy runs it as __main__
    Str file;    // a path, or "-" for stdin
    bool version = false;
    bool stay    = false; // -i: the prompt, over what the program left behind
};

// A dotted name, which is all -m can take: nothing here has to be escaped to
// go inside the string literal that runs it.
bool module_name_ok(Str s)
{
    if (s.empty())
        return false;
    for (usize i = 0; i < s.size(); i++) {
        char c  = s[i];
        bool ok = c == '.' || c == '_' || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
                  (c >= 'A' && c <= 'Z');
        if (!ok)
            return false;
    }
    return true;
}

// How this binary was named on the command line, for sys.executable when the
// store cannot say better. It views argv, which outlives the process's work.
Str argv0 = "python";

// The pending error as a traceback reads it: for a SyntaxError, the file and
// the line it was found at, the text of that line and a caret under the
// column. False when there is no exception object to be had.
bool pending_text(Str filename, Str source, String &out)
{
    py_init(); // an exception object needs the types, which vm_start makes
    err_set_file(filename, source);
    Value e = exc_pending();
    if (e.is_nil())
        return false;
    Root re{ e };
    err_clear();
    return exc_where(re.v, out) && exc_line(re.v, out) && out.push('\n');
}

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

// Is `root`/lib a directory? mbasic's epath.cpp resolves its examples
// the same way, and for the same reason: the store path carries a version the
// binary does not know.
Task<bool> holds_library(Str root, String &out)
{
    if (!out.assign(root) || !out.append("/lib"))
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

// sys.executable: the path this binary was reached by. The installed link
// says it outright; otherwise the name is looked for along PATH, the way the
// shell found it. Empty when neither answers, which is what CPython leaves it
// as when it cannot work the path out either.
Task<void> find_self(String &out)
{
    Result<String> link = Err(Error::NoMemory);
    if (Task<Result<String>> t = read_link("/pkg/bin/python"))
        link = co_await t;
    if (link.is_ok()) {
        out = static_cast<String &&>(link.value());
        co_return;
    }
    if (argv0.find('/') != Str::npos) {
        out.assign(argv0);
        co_return;
    }
    Str path = proc_env("PATH");
    for (usize at = 0; at < path.size();) {
        usize end = at;
        while (end < path.size() && path[end] != ':')
            end++;
        String cand;
        if (path_join(path.substr(at, end - at), argv0, cand).is_ok()) {
            Result<FileInfo> st = Err(Error::NoMemory);
            if (Task<Result<FileInfo>> t = stat_of(cand.str()))
                st = co_await t;
            if (st.is_ok() && st.value().kind == SYS_KIND_FILE) {
                out = static_cast<String &&>(cand);
                co_return;
            }
        }
        at = end + 1;
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
            out          = static_cast<String &&>((*hidden)[i].path);
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

// What the interpreter needs and cannot ask for itself, because each answer is
// an asynchronous syscall and nothing under vm_burst awaits. Called once, just
// after vm_start.
Task<void> settle(Str script)
{
    // Whether the three descriptors are the terminal, and what day it is.
    // time.time() counts on from this reading with Sys::Now, which is
    // monotonic and cannot name a day of its own.
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
    String self;
    co_await find_self(self);
    sys_set_executable(self.str());
}

// The driver loop. It returns where the VM has nothing more to run: the end of
// the program, or -- at a prompt -- the end of one command.
Task<i32> drive()
{
    for (;;) {
        Req r = vm_burst();
        if (r.kind == ReqKind::Exit)
            co_return r.status;
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

// A whole program: compile it, start the VM and run it to the end.
Task<i32> interpret(Str source, Str name, Args argv, Str script, bool stay)
{
    // Collecting at every allocation turns a missing Root into a wrong answer
    // rather than a rare crash. It is slow, so a program asks for it by name.
    if (!proc_env("PY_GC_STRESS").empty())
        gc_stress(true);

    {
        Ast ast;
        Root code;
        if (ast.parse(source))
            code = py_compile(ast, name);
        if (code.v.is_nil() || !vm_start(code.v, argv, script.empty() ? Str() : name)) {
            String out;
            if (pending_text(name, source, out))
                co_await write_all(SYS_STDERR, out.str());
            else
                co_await write_all(SYS_STDERR, where().str());
            co_return 1;
        }
    }
    // -i: the program's end is not the session's, so atexit waits for the
    // prompt to be done with.
    if (stay)
        vm_set_prompt(true);
    co_await settle(script);
    co_return co_await drive();
}

// ------------------------------------------------------------- the prompt
//
// A key ring has exactly one receiver and there is no non-blocking key read,
// so the keyboard changes hands at the one boundary that matters: the editor
// holds it while a command is being typed, and gives it back the moment the
// command runs -- which is when the console's own pump, not the editor, should
// be the thing that turns a ^C into SIG_INT. mbasic solved it the same way.
//
// These outlive every read, so none of them sits in a coroutine frame.
bool console; // stdin is a terminal, so a correctable line is possible
bool holding;
LineEditor *editor;
Input *reader;
LineReader *lines;

Task<bool> keys_take(bool take)
{
    if (!console || take == holding)
        co_return holding;
    Result<Geometry> g = Err(Error::Perm);
    if (Task<Result<Geometry>> t = keys_claim(take))
        g = co_await t;
    if (g.is_ok())
        holding = take;
    co_return holding;
}

// The claim is made at the first read rather than here, because the shell
// gives the keyboard back on its way to running us and a claim that raced it
// would answer Err(Perm) for the whole session.
Task<void> prompt_init()
{
    Result<TtyInfo> tty = Err(Error::Unsupported);
    if (Task<Result<TtyInfo>> t = tty_of(SYS_STDIN))
        tty = co_await t;
    console = tty.is_ok() && tty.value().console;
    if (console) {
        editor = heap_new<LineEditor>();
        if (Task<Result<void>> t = sig_catch(SIG_WINCH))
            co_await t;
    }
    reader = heap_new<Input>(Args{}, SYS_STDIN, WHO);
    if (reader)
        lines = heap_new<LineReader>(*reader);
}

// One line, with the prompt already written. Anything but Enter ends the
// command being typed.
Task<LineEnd> read_line(String &line)
{
    line.clear();
    bool keys = editor && co_await keys_take(true);
    if (keys) {
        Result<InLine> r = Err(Error::NoMemory);
        if (Task<Result<InLine>> t = editor->read_line())
            r = co_await t;
        (void)sig_take(SIG_INT); // the editor took it, so nothing else does
        if (r.is_err())
            co_return LineEnd::Eof;
        if (r.value().how == LineEnd::Enter && !line.assign(r.value().text.str()))
            co_return LineEnd::Eof;
        co_return r.value().how;
    }
    if (!lines)
        co_return LineEnd::Eof;
    Result<bool> r = co_await lines->next(line);
    if (r.is_err() && r.error() == Error::Intr && sig_take(SIG_INT))
        co_return LineEnd::Interrupt;
    co_return r.is_ok() && r.value() ? LineEnd::Enter : LineEnd::Eof;
}

// What a command typed at the prompt turned out to be.
enum class Typed : u8 {
    Done, // it compiled
    More, // the blocks or the brackets are still open
    Bad,  // a SyntaxError, which is pending
};

// One compile. `incomplete` is PyCF_ALLOW_INCOMPLETE_INPUT and
// PyCF_DONT_IMPLY_DEDENT together, which is how codeop asks for them.
Typed compile_once(Str text, Root &code, bool incomplete)
{
    Ast ast;
    ast.lex.keep_indent = incomplete;
    ast.lex.interactive = true;
    bool parsed         = ast.parse(text);
    if (incomplete && ast.lex.wants_more)
        return Typed::More;
    if (!parsed)
        return Typed::Bad;
    code = py_compile(ast, "<stdin>", CompileMode::Single);
    return code.v.is_nil() ? Typed::Bad : Typed::Done;
}

// codeop's rule, which is what tells a command still being typed from a
// mistake: what the same text with a newline after it does. If that compiles,
// or wants more still, the command is unfinished; if it does not, the error
// the text as typed gives is the one to report.
Typed compile_typed(Str text, Root &code)
{
    if (compile_once(text, code, true) == Typed::Done)
        return Typed::Done;
    String more;
    Root ignored;
    if (!more.assign(text) || !more.push('\n'))
        return Typed::Bad;
    if (compile_once(more.str(), ignored, true) != Typed::Bad)
        return Typed::More;
    return compile_once(text, code, false);
}

// How the reading of one command ended.
enum class Cmd : u8 { Ready, Blank, Eof };

// Lines until they make a command. CPython's console joins them with a
// newline and adds none at the end, which is what leaves a suite open until
// an empty line closes it.
Task<Cmd> read_command(String &text)
{
    text.clear();
    for (;;) {
        // The prompt is stderr's, as it is in CPython: a redirected stdout
        // carries what the commands printed and nothing else.
        String p;
        sys_prompt(!text.empty(), p);
        if ((co_await write_all(SYS_STDERR, p.str())).is_err())
            co_return Cmd::Eof;
        String line;
        LineEnd how = co_await read_line(line);
        if (how == LineEnd::Eof)
            co_return text.empty() ? Cmd::Eof : Cmd::Blank;
        if (how == LineEnd::Interrupt) {
            co_await write_all(SYS_STDERR, "KeyboardInterrupt\n");
            co_return Cmd::Blank;
        }
        if (!text.empty() && !text.push('\n'))
            co_return Cmd::Eof;
        if (!text.append(line.str()))
            co_return Cmd::Eof;
        Root code;
        Typed t = compile_typed(text.str(), code);
        if (t == Typed::More)
            continue;
        if (t == Typed::Bad) {
            String out;
            if (pending_text("<stdin>", text.str(), out))
                co_await write_all(SYS_STDERR, out.str());
            err_clear();
            co_return Cmd::Blank;
        }
        if (!vm_again(code.v))
            co_return Cmd::Eof;
        co_return Cmd::Ready;
    }
}

// The read-eval-print loop. `greeting` is false for -i, which has already
// printed whatever the program printed.
Task<i32> repl(bool greeting)
{
    co_await prompt_init();
    if (!sys_set_prompts())
        co_return 1;
    if (greeting) {
        Buf<64> b;
        b.put(VERSION).put('\n');
        co_await write_all(SYS_STDERR, b.str());
    }
    vm_set_prompt(true);
    i32 status = 0;
    for (;;) {
        String text;
        Cmd c = co_await read_command(text);
        if (c == Cmd::Eof)
            break;
        if (c == Cmd::Blank)
            continue;
        // The command runs with the keyboard back where the console can see a
        // ^C, which is what makes a long loop interruptible.
        co_await keys_take(false);
        status = co_await drive();
        if (vm_quitting())
            break;
    }
    co_await keys_take(false);
    // The newline ends the prompt the end of input was typed at; a command
    // that asked to leave has already ended its own line.
    if (!vm_quitting())
        co_await write_all(SYS_STDERR, "\n");
    vm_finish();
    i32 last = co_await drive();
    co_return vm_quitting() ? last : status;
}

} // namespace

Task<i32> proc_main(Args args)
{
    // Before the first park, which is reading the program: a signal that
    // arrives before this is acted on rather than delivered, and the process
    // simply goes.
    co_await sig_catch(SIG_INT);
    if (args.size())
        argv0 = args[0];

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
        if (o.name == 'i')
            job.stay = true;
        // -m and -c end the options: what follows is the program's.
        if (o.name == 'm') {
            job.module = o.value;
            break;
        }
        if (o.name == 'c') {
            job.command = o.value;
            break;
        }
    }

    Args rest = opts.rest();
    if (job.command.empty() && job.module.empty() && rest.size() > 0)
        job.file = rest[0];

    if (job.version)
        co_return co_await banner();

    // No program at all. A terminal means a prompt, over a __main__ that has
    // run nothing; a pipe or a file means the program is what stdin holds,
    // which is what CPython does with it too.
    if (job.command.empty() && job.module.empty() && job.file.empty()) {
        Result<TtyInfo> tty = Err(Error::Unsupported);
        if (Task<Result<TtyInfo>> t = tty_of(SYS_STDIN))
            tty = co_await t;
        // -i asks for the prompt whatever stdin is.
        if (job.stay || (tty.is_ok() && tty.value().console)) {
            i32 status = co_await interpret(Str(), "<stdin>", rest, Str(), true);
            if (status == 0)
                status = co_await repl(!job.stay);
            co_await hidden_sweep();
            co_return status;
        }
        job.file = "-";
    }

    // The source, and the name the traceback will carry. `-` is stdin, which
    // Input does not spell that way: it takes no path at all for that.
    String source;
    Str name = "<string>";
    Vec<Str> argv;
    if (!job.module.empty()) {
        if (!module_name_ok(job.module))
            co_return co_await complain("not a module name", job.module);
        // PEP 338, through runpy itself: the module is found the way an import
        // finds it, and __name__ is "__main__" while it runs.
        if (!source.append("import runpy\nrunpy._run_module_as_main(\"") ||
            !source.append(job.module) || !source.append("\")\n"))
            co_return co_await complain("out of memory", Str());
        // sys.argv[0] is the module until runpy replaces it with its file.
        if (!argv.push(job.module))
            co_return co_await complain("out of memory", Str());
        for (usize i = 0; i < rest.size(); i++)
            if (!argv.push(rest[i]))
                co_return co_await complain("out of memory", Str());
        rest = Args{ Span<const Str>(argv.data(), argv.size()) };
    } else if (!job.command.empty()) {
        if (!source.append(job.command))
            co_return co_await complain("out of memory", Str());
        // sys.argv[0] is "-c" itself, as it is in CPython.
        if (!argv.push("-c"))
            co_return co_await complain("out of memory", Str());
        for (usize i = 0; i < rest.size(); i++)
            if (!argv.push(rest[i]))
                co_return co_await complain("out of memory", Str());
        rest = Args{ Span<const Str>(argv.data(), argv.size()) };
    } else {
        bool dash = job.file == "-";
        name      = dash ? Str("<stdin>") : job.file;
        if (!co_await slurp(Args{ rest.v.subspan(0, dash ? 0 : 1) }, source))
            co_return 1;
    }

    bool from_file = job.command.empty() && job.module.empty() && job.file != "-";
    i32 status =
        co_await interpret(source.str(), name, rest, from_file ? job.file : Str(), job.stay);
    // -i: the prompt takes over where the program left off, over the same
    // __main__. A traceback does not stop that, as it does not in CPython.
    if (job.stay && !vm_quitting())
        status = co_await repl(false);
    co_await hidden_sweep();
    co_return status;
}
