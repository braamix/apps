// The Braam platform: proc_main, and every co_await in the program. The
// interpreter under it stays plain C++ and hands the driver what to do --
// simbesm's cpu_burst() shape, and for the same reason: a co_await is a call
// and not a tail call.
//
// The command line, the three ways a program arrives, and the loop that
// performs what vm_burst asks for. README.md says how the pieces fit.
#include "bigint.h"
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
#include "tracemalloc.h"
#include "vm.h"

namespace {

constexpr Str WHO = "python";

// The language version, which is also what sys.version says. The port's own
// number is the package's.
constexpr Str VERSION = "Python 3.14.0 on Braam";

constexpr Str USAGE =
    "Usage:\n"
    "    python                   read commands at a prompt\n"
    "    python <file> [arg]...   run the program in <file>\n"
    "    python -c <cmd> [arg]... run <cmd>\n"
    "    python -m <mod> [arg]... run module <mod> as __main__\n"
    "    python - [arg]...        read the program from stdin\n"
    "    python -i ...            keep the prompt when the program ends\n"
    "    python -V                print the version\n"
    "\n"
    "    -b, -bb    warn, or fail, on str(bytes) and on bytes == str\n"
    "    -B, -s     accepted: nothing writes bytecode, and there is no user site\n"
    "    -E         ignore the PYTHON* variables\n"
    "    -I         isolated: -E, -P and -s together\n"
    "    -O, -OO    drop asserts and __debug__; -OO drops docstrings too\n"
    "    -P         leave the program's directory off sys.path\n"
    "    -q         no banner at the prompt\n"
    "    -S         do not import site\n"
    "    -u         write stdout and stderr at once\n"
    "    -v         say what each import loads, -vv for more\n"
    "    -W <arg>   a warnings filter, as sys.warnoptions\n"
    "    -X <opt>   dev, utf8, importtime, int_max_str_digits=<n>,\n"
    "               tracemalloc[=<n>], warn_default_encoding; any other is\n"
    "               kept in sys._xoptions\n"
    "    python --dump-tokens <f> print the token stream of <f>\n"
    "    python --dump-ast <f>    print the parse tree of <f>\n"
    "    python --dis <f>         print the bytecode of <f>\n"
    "\n"
    "Python 3, written for Braam: its own compiler, its own bytecode and its\n"
    "own virtual machine -- see Manual.md for the language and its limits.\n";

constexpr Opts SPEC = { "VibBEIOPqsSuv", "cmWX" };

// What the command line settles.
struct Job {
    Str command; // -c, and then source is the command itself
    Str module;  // -m, and then runpy runs it as __main__
    Str file;    // a path, or "-" for stdin
    bool version = false;
    bool stay    = false; // -i: the prompt, over what the program left behind
    Vec<Str> warn;        // -W, which go after PYTHONWARNINGS
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

// A PYTHON* variable, or empty under -E and -I.
Str env(Str name)
{
    return py_config().ignore_env ? Str() : proc_env(name);
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

// The words of a list each ended by a NUL, as views into it.
bool split_words(Str blob, Vec<Str> &out)
{
    usize at = 0;
    while (at < blob.size()) {
        usize end = at;
        while (end < blob.size() && blob[end])
            end++;
        if (!out.push(blob.substr(at, end - at)))
            return false;
        at = end + 1;
    }
    return true;
}

// Whether slot `i` may share `fd` rather than be handed one of its own.
bool shares(u32 i, i32 fd)
{
    if (fd < 0)
        return true;
    if (fd >= i32(SYS_FD_MIN))
        return false;
    return i == 0 ? fd == 0 : fd == 1 || fd == 2;
}

// What a spawn keeps while it runs, too much for a frame. A spawn moves a
// descriptor out of this table, and the program closes its own ends itself, so
// each slot gets a copy. `made` are those copies, -1 where there is none.
struct Spawning {
    Vec<Str> argv, env;
    String home;
    i32 made[3] = { -1, -1, -1 };
};

// The children spawned and not yet waited for, so that a wait for any of
// them that must not park can ask after each.
Vec<u32> *children;

void child_gone(u32 pid)
{
    for (usize i = 0; children && i < children->size(); i++)
        if ((*children)[i] == pid) {
            (*children)[i] = (*children)[children->size() - 1];
            children->pop();
            return;
        }
}

// Spawn: the child, with the working directory moved there and back around it,
// since a child starts where its parent is. A failed chdir answers kind 1.
Task<void> sys_spawn(const SysReq &q, SysAns &a)
{
    Spawning *w = heap_new<Spawning>();
    if (!w) {
        fail(a, Error::NoMemory);
        co_return;
    }
    u32 slot[3] = { SYS_STDIN, SYS_STDOUT, SYS_STDERR };
    bool moved  = false;
    bool away   = false;
    if (!split_words(q.data, w->argv) || !split_words(q.path2, w->env)) {
        fail(a, Error::NoMemory);
        goto out;
    }
    if (w->argv.empty()) {
        fail(a, Error::Invalid);
        goto out;
    }
    for (u32 i = 0; i < 3; i++) {
        if (shares(i, q.io[i])) {
            if (q.io[i] >= 0)
                slot[i] = u32(q.io[i]);
            continue;
        }
        Result<u32> d = Err(Error::NoMemory);
        if (Task<Result<u32>> t = dup_fd(u32(q.io[i])))
            d = co_await t;
        if (d.is_err()) {
            fail(a, d.error());
            goto out;
        }
        w->made[i] = i32(d.value());
        slot[i]    = d.value();
    }
    if (!q.path.empty()) {
        Result<String> h = Err(Error::NoMemory);
        if (Task<Result<String>> t = cwd_get())
            h = co_await t;
        if (h.is_ok())
            w->home = static_cast<String &&>(h.value());
        Result<String> c = Err(Error::NoMemory);
        if (h.is_ok())
            if (Task<Result<String>> t = cwd_set(q.path))
                c = co_await t;
        if (c.is_err()) {
            fail(a, c.error());
            a.kind = 1;
            goto out;
        }
        away = true;
    }
    {
        Args env{ w->env };
        Result<u32> r = Err(Error::NoMemory);
        if (Task<Result<u32>> t = spawn(Args{ w->argv }, ChildIo{ slot[0], slot[1], slot[2] },
                                        q.flags & SYS_SPAWN_WITH_ENV ? &env : nullptr))
            r = co_await t;
        if (r.is_ok()) {
            a.ok  = true;
            a.n   = r.value();
            moved = true;
            if (!children)
                children = heap_new<Vec<u32>>();
            if (children)
                (void)children->push(r.value());
        } else {
            fail(a, r.error());
        }
    }
out:
    if (away)
        if (Task<Result<String>> t = cwd_set(w->home.str()))
            co_await t;
    for (u32 i = 0; i < 3 && !moved; i++)
        if (w->made[i] >= 0)
            co_await sys_call(Sys::Close, u32(w->made[i]));
    heap_delete(w);
}

// Whether `pid` is still running: its /proc entry goes when it ends.
Task<bool> running(u32 pid)
{
    Buf<24> p;
    p.put("/proc/").put(pid);
    Result<FileInfo> st = Err(Error::NoMemory);
    if (Task<Result<FileInfo>> t = stat_of(p.str()))
        st = co_await t;
    co_return st.is_ok();
}

// Wait. There is no asking whether a child has ended, so a wait that must not
// park looks for one that has, and then waits for that one, which answers at
// once.
Task<void> sys_wait(const SysReq &q, SysAns &a)
{
    u32 pid = q.fd > 0 ? u32(q.fd) : SYS_WAIT_ANY;
    if (q.flags & SYS_WAIT_NOHANG) {
        bool found = false;
        for (usize i = 0; children && i < children->size() && !found; i++) {
            u32 c = (*children)[i];
            if (pid != SYS_WAIT_ANY && c != pid)
                continue;
            Task<bool> t = running(c);
            if (!t) {
                fail(a, Error::NoMemory);
                co_return;
            }
            if (co_await t) {
                if (pid != SYS_WAIT_ANY)
                    break;
                continue;
            }
            pid   = c;
            found = true;
        }
        // Nothing has ended: none, when there is something to wait for.
        if (!found && children && !children->empty()) {
            bool known = pid == SYS_WAIT_ANY;
            for (usize i = 0; i < children->size() && !known; i++)
                known = (*children)[i] == pid;
            if (known) {
                a.ok = true;
                a.n  = 0;
                co_return;
            }
        }
    }
    Result<Exited> r = Err(Error::NoMemory);
    if (Task<Result<Exited>> t = wait_child(pid))
        r = co_await t;
    if (r.is_ok()) {
        a.ok  = true;
        a.n   = r.value().pid;
        a.off = r.value().status;
        child_gone(r.value().pid);
    } else {
        fail(a, r.error());
    }
}

// Poll. The pairs arrive packed into `data` and the revents go back the same
// way; the bits are the kernel's, so nothing is translated in between.
static_assert(SYS_POLL_PAIR == 2 * sizeof(u32));

u32 get_u32(Str s, usize at)
{
    const u8 *p = reinterpret_cast<const u8 *>(s.data()) + at;
    return u32(p[0]) | u32(p[1]) << 8 | u32(p[2]) << 16 | u32(p[3]) << 24;
}

bool put_u32(String &s, u32 v)
{
    return s.push(char(v)) && s.push(char(v >> 8)) && s.push(char(v >> 16)) &&
           s.push(char(v >> 24));
}

Task<void> sys_poll(const SysReq &q, SysAns &a)
{
    usize n = q.data.size() / SYS_POLL_PAIR;
    Vec<PollFd> fds;
    if (!fds.resize(n)) {
        fail(a, Error::NoMemory);
        co_return;
    }
    for (usize i = 0; i < n; i++) {
        fds[i].fd     = get_u32(q.data, i * SYS_POLL_PAIR);
        fds[i].events = get_u32(q.data, i * SYS_POLL_PAIR + sizeof(u32));
    }
    Result<usize> r = Err(Error::NoMemory);
    if (Task<Result<usize>> t = poll_fds(Span<PollFd>(fds.data(), n), q.max))
        r = co_await t;
    if (r.is_err()) {
        fail(a, r.error());
        co_return;
    }
    for (usize i = 0; i < n; i++)
        if (!put_u32(a.data, fds[i].revents)) {
            fail(a, Error::NoMemory);
            co_return;
        }
    a.ok = true;
    a.n  = i64(r.value());
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
    case SysOp::Spawn:
        t = sys_spawn(q, a);
        break;
    case SysOp::Wait:
        t = sys_wait(q, a);
        break;
    case SysOp::Poll:
        t = sys_poll(q, a);
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
Task<void> settle()
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
    // sys.path: PYTHONPATH, then the shipped library. The directory the
    // program came from goes in front once site has run.
    Result<String> cwd = Err(Error::NoMemory);
    if (Task<Result<String>> q = cwd_get())
        cwd = co_await q;
    sys_set_cwd(cwd.is_ok() ? cwd.value().str() : Str("/"));
    String lib;
    co_await find_library(lib);
    sys_set_path(env("PYTHONPATH"), lib.str());
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

// Compile `source` and run it over the __main__ that is already there. The
// answer is the status, and a SyntaxError is reported here.
Task<i32> run_more(Str source, Str name)
{
    bool started = false;
    {
        Ast ast;
        Root code;
        if (ast.parse(source))
            code = py_compile(ast, name);
        started = !code.v.is_nil() && vm_again(code.v);
    }
    if (started)
        co_return co_await drive();
    String out;
    if (pending_text(name, source, out))
        co_await write_all(SYS_STDERR, out.str());
    else
        co_await write_all(SYS_STDERR, where().str());
    err_clear();
    co_return 1;
}

// A whole program: start the VM, import site, then compile the program and
// run it to the end. site comes first, as in CPython, and sees a sys.path
// without the program's own directory in it.
Task<i32> interpret(Str source, Str name, Args argv, Str script, bool stay)
{
    // Collecting at every allocation turns a missing Root into a wrong answer
    // rather than a rare crash. It is slow, so a program asks for it by name.
    if (!proc_env("PY_GC_STRESS").empty())
        gc_stress(true);

    // A library without site.py, as a test may plant, is taken as -S.
    Str boot = py_config().no_site ? Str()
                                   : Str("try:\n"
                                         "    __import__('site')\n"
                                         "except ModuleNotFoundError as _e:\n"
                                         "    if _e.name != 'site':\n"
                                         "        raise\n");
    {
        Ast ast;
        Root code;
        if (ast.parse(boot))
            code = py_compile(ast, "<startup>");
        if (code.v.is_nil() || !vm_start(code.v, argv, script.empty() ? Str() : name))
            co_return co_await complain("cannot start", err_message());
    }
    // The start is a command of its own, so the program's end is not reached
    // at its end.
    vm_set_prompt(true);
    co_await settle();
    i32 started = co_await drive();
    if (vm_quitting())
        co_return co_await drive();
    // A start that raised -- a ^C that came during it, say -- is the end of
    // the program, which does not run.
    if (started != 0) {
        vm_finish();
        (void)co_await drive();
        co_return started;
    }
    // -P, or -I, leaves the program's directory off sys.path. A `-c` or a
    // pipe has no directory of its own, and gets the cwd.
    if (!py_config().safe_path)
        sys_path_first(script.empty() ? Str(".") : path_dirname(script));
    // -i: the program's end is not the session's, so atexit waits for the
    // prompt to be done with.
    vm_set_prompt(stay);
    co_return co_await run_more(source, name);
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

// PYTHONSTARTUP, run over __main__ before the first prompt. A file that is
// not there is said so and the session goes on, as CPython's does.
Task<void> startup()
{
    Str path = env("PYTHONSTARTUP");
    if (path.empty())
        co_return;
    Result<String> text = Err(Error::NoMemory);
    if (Task<Result<String>> t = read_file(path))
        text = co_await t;
    if (text.is_err()) {
        Buf<192> b;
        b.put("Could not open PYTHONSTARTUP\n");
        co_await write_all(SYS_STDERR, b.str());
        co_return;
    }
    (void)co_await run_more(text.value().str(), path);
}

// The read-eval-print loop. `greeting` is false for -i, which has already
// printed whatever the program printed.
Task<i32> repl(bool greeting)
{
    co_await prompt_init();
    if (!sys_set_prompts())
        co_return 1;
    if (greeting && !py_config().quiet) {
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

// PYTHON* as a count: a number is itself, anything else not empty is one.
u32 env_level(Str name)
{
    Str v = env(name);
    if (v.empty())
        return 0;
    u32 n = 0;
    for (usize i = 0; i < v.size(); i++) {
        if (v[i] < '0' || v[i] > '9')
            return 1;
        n = n * 10 + u32(v[i] - '0');
    }
    return n ? n : 1;
}

// A comma-separated list of warning filters, PYTHONWARNINGS's form.
bool add_warnings(Str list)
{
    while (!list.empty()) {
        usize comma = list.find(',');
        Str w       = comma == Str::npos ? list : list.substr(0, comma);
        list        = comma == Str::npos ? Str() : list.substr(comma + 1);
        if (!w.empty() && !py_config().warnoptions.push(w))
            return false;
    }
    return true;
}

// The -X option called `name`: its value, "" for a bare one, or false.
bool xoption(Str name, Str &value)
{
    for (Str x : py_config().xoptions) {
        usize eq = x.find('=');
        if ((eq == Str::npos ? x : x.substr(0, eq)) == name) {
            value = eq == Str::npos ? Str() : x.substr(eq + 1);
            return true;
        }
    }
    return false;
}

// A digit limit as -X or the environment spells it; -1 for a bad one.
i64 digits_of(Str s)
{
    i64 n = 0;
    if (s.empty() || s.size() > 10)
        return -1;
    for (usize i = 0; i < s.size(); i++) {
        if (s[i] < '0' || s[i] > '9')
            return -1;
        n = n * 10 + (s[i] - '0');
    }
    return n != 0 && (n < 640 || n > 0x7fffffff) ? -1 : n;
}

// A traceback depth as -X tracemalloc or the environment spells it; -1 for a
// bad one, and 0 for the PYTHONTRACEMALLOC=0 that asks for nothing.
i64 nframes_of(Str s)
{
    i64 n = 0;
    if (s.empty() || s.size() > 5)
        return -1;
    for (usize i = 0; i < s.size(); i++) {
        if (s[i] < '0' || s[i] > '9')
            return -1;
        n = n * 10 + (s[i] - '0');
    }
    return n > i64(TM_MAX_NFRAME) ? -1 : n;
}

// The environment over the command line's defaults, and what both settle
// put where the interpreter reads it. Nonzero is a status to leave with.
Task<i32> configure(Job &job)
{
    PyConfig &c = py_config();
    Str v;
    c.optimize =
        c.optimize > env_level("PYTHONOPTIMIZE") ? c.optimize : env_level("PYTHONOPTIMIZE");
    c.verbose = c.verbose > env_level("PYTHONVERBOSE") ? c.verbose : env_level("PYTHONVERBOSE");
    if (!env("PYTHONUNBUFFERED").empty())
        c.unbuffered = true;
    if (!env("PYTHONSAFEPATH").empty())
        c.safe_path = true;
    if (!env("PYTHONINSPECT").empty())
        job.stay = true;
    if (!env("PYTHONDEVMODE").empty() || xoption("dev", v))
        c.dev_mode = true;
    if (!env("PYTHONPROFILEIMPORTTIME").empty() || xoption("importtime", v))
        c.import_time = true;
    if (!env("PYTHONWARNDEFAULTENCODING").empty() || xoption("warn_default_encoding", v))
        c.warn_default_encoding = 1;
    if (job.stay)
        c.inspect = c.interactive = true;
    if (c.optimize > 2)
        c.optimize = 2;

    // Dev mode's own filter, then PYTHONWARNINGS, then -W: the later wins.
    if ((c.dev_mode && !c.warnoptions.push("default")) || !add_warnings(env("PYTHONWARNINGS")))
        co_return co_await complain("out of memory", Str());
    for (Str w : job.warn)
        if (!c.warnoptions.push(w))
            co_return co_await complain("out of memory", Str());

    Str digits = env("PYTHONINTMAXSTRDIGITS");
    Str who    = "PYTHONINTMAXSTRDIGITS";
    if (xoption("int_max_str_digits", v)) {
        digits = v;
        who    = "-X int_max_str_digits";
    }
    if (!digits.empty() || who != "PYTHONINTMAXSTRDIGITS") {
        i64 n = digits_of(digits);
        if (n < 0)
            co_return co_await complain(who, "invalid limit; must be >= 640 or 0 for unlimited");
        c.int_max_str_digits = i32(n);
        int_set_max_str_digits(u32(n));
    }

    // Tracing starts before the program does, so what the interpreter itself
    // is still holding is traced too, as it is in CPython. A bare -X is one
    // frame; PYTHONTRACEMALLOC=0 is off.
    Str frames = env("PYTHONTRACEMALLOC");
    Str asked  = frames.empty() ? Str() : Str("PYTHONTRACEMALLOC");
    if (xoption("tracemalloc", v)) {
        frames = v.empty() ? Str("1") : v;
        asked  = "-X tracemalloc=NFRAME";
    }
    if (!asked.empty()) {
        i64 n = nframes_of(frames);
        if (n < 0)
            co_return co_await complain(asked, "invalid number of frames");
        if (n > 0 && !tm_start(u32(n)))
            co_return co_await complain("out of memory", Str());
    }

    compile_set_optimize(c.optimize);
    vm_set_unbuffered(c.unbuffered);
    co_return 0;
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
        PyConfig &c = py_config();
        switch (o.name) {
        case 'V':
            job.version = true;
            break;
        case 'i':
            job.stay = true;
            break;
        case 'b':
            c.bytes_warning++;
            break;
        case 'E':
            c.ignore_env = true;
            break;
        case 'I':
            c.isolated = c.ignore_env = c.safe_path = true;
            break;
        case 'O':
            c.optimize++;
            break;
        case 'P':
            c.safe_path = true;
            break;
        case 'q':
            c.quiet = true;
            break;
        case 'S':
            c.no_site = true;
            break;
        case 'u':
            c.unbuffered = true;
            break;
        case 'v':
            c.verbose++;
            break;
        case 'W':
            if (!job.warn.push(o.value))
                co_return co_await complain("out of memory", Str());
            break;
        case 'X':
            if (!c.xoptions.push(o.value))
                co_return co_await complain("out of memory", Str());
            break;
        default:
            break;
        }
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
    if (i32 bad = co_await configure(job))
        co_return bad;

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
                co_await startup();
            if (status == 0 && !vm_quitting())
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
