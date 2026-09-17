// `_signal`: what signal.py stands on.
//
// Braam delivers three signals -- ^C, SIGTERM and SIGWINCH -- and only to a
// process that asked for them. The driver asks for ^C before the program
// starts, so it can become KeyboardInterrupt; a handler the program installs
// for either of the other two asks the kernel through the driver. A delivered
// signal reaches the VM between two instructions, and its handler is called
// there, as CPython calls one; an exception out of it is raised where the
// program was.
#include "posix.h"

#include "builtin.h"
#include "exc.h"
#include "gc.h"
#include "intern.h"
#include "kernel/alloc.h"
#include "kernel/fmt.h"
#include "kernel/sysabi.h"
#include "module.h"
#include "ops.h"
#include "type.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

constexpr i64 NSIG_   = 65;
constexpr i64 SIG_DFL_ = 0;
constexpr i64 SIG_IGN_ = 1;

struct Home {
    Value handlers[NSIG_]; // Nil for SIG_DFL
    Value default_int;     // default_int_handler
};

Home *home;

void home_mark()
{
    if (!home)
        return;
    for (Value v : home->handlers)
        gc_mark(v);
    gc_mark(home->default_int);
}

Home *here()
{
    if (!home) {
        home = heap_new<Home>();
        if (home)
            gc_root_hook(home_mark);
    }
    return home;
}

R b_default_int_handler(const CallArgs &a, Value &out)
{
    (void)a;
    out = Value();
    return err_set("KeyboardInterrupt", "");
}

// The handler for SIGINT, before the program changed it.
Value default_int()
{
    Home *h = here();
    if (!h)
        return oom(), Value();
    if (h->default_int.is_nil()) {
        h->default_int = native_new("default_int_handler", b_default_int_handler);
        if (!h->default_int.is_nil())
            h->default_int.obj()->flags |= OBJ_PLAINFN;
        h->handlers[SIG_INT] = h->default_int;
    }
    return h->default_int;
}

struct SigName {
    Str name;
    i64 n;
};

// Linux's numbers, which are also the kernel's.
constexpr SigName SIGNALS[] = {
    { "SIGHUP", 1 },    { "SIGINT", 2 },   { "SIGQUIT", 3 },  { "SIGILL", 4 },
    { "SIGTRAP", 5 },   { "SIGABRT", 6 },  { "SIGIOT", 6 },   { "SIGBUS", 7 },
    { "SIGFPE", 8 },    { "SIGKILL", 9 },  { "SIGUSR1", 10 }, { "SIGSEGV", 11 },
    { "SIGUSR2", 12 },  { "SIGPIPE", 13 }, { "SIGALRM", 14 }, { "SIGTERM", 15 },
    { "SIGCHLD", 17 },  { "SIGCONT", 18 }, { "SIGSTOP", 19 }, { "SIGTSTP", 20 },
    { "SIGTTIN", 21 },  { "SIGTTOU", 22 }, { "SIGURG", 23 },  { "SIGXCPU", 24 },
    { "SIGXFSZ", 25 },  { "SIGVTALRM", 26 }, { "SIGPROF", 27 }, { "SIGWINCH", 28 },
    { "SIGIO", 29 },    { "SIGPOLL", 29 }, { "SIGPWR", 30 },  { "SIGSYS", 31 },
};

constexpr Str DESCRIPTIONS[] = {
    "",
    "Hangup",
    "Interrupt",
    "Quit",
    "Illegal instruction",
    "Trace/breakpoint trap",
    "Aborted",
    "Bus error",
    "Floating point exception",
    "Killed",
    "User defined signal 1",
    "Segmentation fault",
    "User defined signal 2",
    "Broken pipe",
    "Alarm clock",
    "Terminated",
    "Stack fault",
    "Child exited",
    "Continued",
    "Stopped (signal)",
    "Stopped",
    "Stopped (tty input)",
    "Stopped (tty output)",
    "Urgent I/O condition",
    "CPU time limit exceeded",
    "File size limit exceeded",
    "Virtual timer expired",
    "Profiling timer expired",
    "Window changed",
    "I/O possible",
    "Power failure",
    "Bad system call",
};

bool signum_of(Value v, i64 &n)
{
    if (!as_int_arg(v, n)) {
        err_set2("TypeError", "an integer is required", type_name(v));
        return false;
    }
    if (n < 1 || n >= NSIG_) {
        err_set("ValueError", "signal number out of range");
        return false;
    }
    return true;
}

// What a slot holds, as Python sees it.
Value shown(Value h)
{
    return h.is_nil() ? Value::of_int(i32(SIG_DFL_)) : h;
}

R b_getsignal(const CallArgs &a, Value &out)
{
    i64 n = 0;
    if (!args_only(a, "getsignal", 1, 1) || !signum_of(a.args[0], n))
        return R::Err;
    if (default_int().is_nil())
        return R::Err;
    out = shown(here()->handlers[n]);
    return R::Ok;
}

// s[0] the old handler, j the signal, a flag whether the kernel is to be told.
R catch_step(ContObj *k, Value)
{
    SysReq q;
    q.op    = SysOp::SigCatch;
    q.fd    = i32(k->j);
    q.flags = k->s[1].is_nil() ? 0 : 1;
    R r;
    if (!sys_turn(k, q, r))
        return r;
    return cont_done(k, k->s[0]);
}

R b_signal(const CallArgs &a, Value &out)
{
    i64 n = 0;
    if (!args_only(a, "signal", 2, 2) || !signum_of(a.args[0], n))
        return R::Err;
    if (default_int().is_nil())
        return R::Err;
    Value h = a.args[1];
    i64 d   = -1;
    if (as_int_arg(h, d)) {
        if (d != SIG_DFL_ && d != SIG_IGN_)
            return err_set("TypeError",
                           "signal handler must be signal.SIG_IGN, signal.SIG_DFL, or a callable "
                           "object");
    } else if (!py_callable(h)) {
        return err_set("TypeError",
                       "signal handler must be signal.SIG_IGN, signal.SIG_DFL, or a callable "
                       "object");
    }
    if (n == SIG_KILL || n == 19)
        return err_errno(22);
    Home *hm = here();
    Root old{ shown(hm->handlers[n]) };
    Value next = d == SIG_DFL_ ? Value() : h;
    // Whether the kernel delivers it at all changes only across SIG_DFL, and
    // only for the three it can deliver.
    bool was = !hm->handlers[n].is_nil();
    bool now = !next.is_nil();
    hm->handlers[n] = next;
    if (was == now || !((SIG_CATCHABLE >> n) & 1)) {
        out = old.v;
        return R::Ok;
    }
    Root kv{ cont_new(catch_step) };
    if (kv.v.is_nil())
        return R::Err;
    cont_of(kv.v)->s[0] = old.v;
    cont_of(kv.v)->s[1] = now ? value_bool(true) : Value();
    cont_of(kv.v)->j    = u32(n);
    out                 = kv.v;
    return R::Ok;
}

R b_raise_signal(const CallArgs &a, Value &out)
{
    i64 n = 0;
    if (!args_only(a, "raise_signal", 1, 1))
        return R::Err;
    if (!as_int_arg(a.args[0], n))
        return err_set2("TypeError", "an integer is required", type_name(a.args[0]));
    // raise(3): 0 is the null signal, and past the table is EINVAL.
    if (n == 0)
        return out = value_none(), R::Ok;
    if (n < 0 || n >= NSIG_)
        return err_errno(22);
    if (default_int().is_nil())
        return R::Err;
    Value h = here()->handlers[n];
    if (h.is_nil()) {
        // The default action: what the kernel would do, which is to end the
        // process, and for anything it has no number for, the same.
        if (n == 28 || n == 17 || n == 18 || n == 23)
            return out = value_none(), R::Ok;
        vm_hard_exit(i32(128 + n));
        out = value_none();
        return R::Ok;
    }
    vm_signal(u32(n));
    out = value_none();
    return R::Ok;
}

R b_strsignal(const CallArgs &a, Value &out)
{
    i64 n = 0;
    if (!args_only(a, "strsignal", 1, 1) || !signum_of(a.args[0], n))
        return R::Err;
    if (n >= i64(sizeof DESCRIPTIONS / sizeof DESCRIPTIONS[0])) {
        out = value_none();
        return R::Ok;
    }
    out = str_new(DESCRIPTIONS[n]);
    return out.is_nil() ? R::Err : R::Ok;
}

R b_valid_signals(const CallArgs &a, Value &out)
{
    if (!args_only(a, "valid_signals", 0, 0))
        return R::Err;
    SetObj *s = set_new();
    if (!s)
        return oom();
    Root rs{ obj_value(s) };
    for (i32 i = 1; i < 32; i++)
        if (i != 16 && set_add(static_cast<SetObj *>(rs.v.obj()), Value::of_int(i)) != R::Ok)
            return R::Err;
    out = rs.v;
    return R::Ok;
}

R b_set_wakeup_fd(const CallArgs &a, Value &out)
{
    (void)a;
    out = Value::of_int(-1);
    return R::Ok;
}

R b_siginterrupt(const CallArgs &a, Value &out)
{
    i64 n = 0;
    if (!args_only(a, "siginterrupt", 2, 2) || !signum_of(a.args[0], n))
        return R::Err;
    out = value_none();
    return R::Ok;
}

constexpr ModDef DEFS[] = {
    { "signal", b_signal },
    { "getsignal", b_getsignal },
    { "raise_signal", b_raise_signal },
    { "strsignal", b_strsignal },
    { "valid_signals", b_valid_signals },
    { "set_wakeup_fd", b_set_wakeup_fd },
    { "siginterrupt", b_siginterrupt },
};

} // namespace

R sig_handler(u32 sig, Value &out)
{
    out = Value();
    if (default_int().is_nil())
        return R::Err;
    Value h = sig < NSIG_ ? here()->handlers[sig] : Value();
    i64 d   = -1;
    if (h.is_nil()) {
        // SIG_DFL: the kernel ends the process for these, so only a signal
        // raised by the program itself arrives here.
        if (sig == SIG_INT)
            return err_set("KeyboardInterrupt", "");
        return R::Ok;
    }
    if (as_int_arg(h, d))
        return R::Ok; // SIG_IGN
    if (h == here()->default_int)
        return err_set("KeyboardInterrupt", "");
    out = h;
    return R::Ok;
}

R sig_interrupted(Value &out)
{
    return sig_handler(SIG_INT, out);
}

bool signal_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    DictObj *d = static_cast<DictObj *>(rd.v.obj());
    Root di{ default_int() };
    if (di.v.is_nil() || !mod_defs(d, DEFS) || !mod_put(d, "default_int_handler", di.v))
        return false;
    for (const SigName &s : SIGNALS)
        if (!mod_int(d, s.name, s.n))
            return false;
    return mod_int(d, "SIG_DFL", SIG_DFL_) && mod_int(d, "SIG_IGN", SIG_IGN_) &&
           mod_int(d, "NSIG", NSIG_);
}
