// `select`: the floor under selectors, and so under subprocess.
//
// Both calls are Sys::Poll. The kernel answers for pipes, for the stdio
// behind them and for files, which are always ready; a descriptor that waits
// on a host call rather than on a channel is an OSError. epoll and kqueue are
// not here, which is what makes selectors choose PollSelector.
#include "builtin.h"
#include "exc.h"
#include "gc.h"
#include "intern.h"
#include "kernel/sysabi.h"
#include "method.h"
#include "module.h"
#include "ops.h"
#include "posix.h"
#include "proc/rt.h"
#include "type.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

// The module's bits are Linux's, because a program that spells out a mask
// expects those; the kernel's three are its own. Everything that crosses the
// system call is translated, and PRI, ERR and NVAL never come back.
enum : u32 {
    POLLIN   = 1,
    POLLPRI  = 2,
    POLLOUT  = 4,
    POLLERR  = 8,
    POLLHUP  = 16,
    POLLNVAL = 32,
};

u32 events_out(u32 mask)
{
    return (mask & POLLIN ? SYS_POLL_IN : 0) | (mask & POLLOUT ? SYS_POLL_OUT : 0);
}

u32 events_in(u32 revents)
{
    return (revents & SYS_POLL_IN ? POLLIN : 0) | (revents & SYS_POLL_OUT ? POLLOUT : 0) |
           (revents & SYS_POLL_HUP ? POLLHUP : 0);
}

// One descriptor of a request, as the payload lays it out.
bool pair_push(String &s, u32 fd, u32 events)
{
    u32 w[2] = { fd, events };
    for (u32 v : w)
        for (u32 i = 0; i < 4; i++)
            if (!s.push(char(v >> (8 * i))))
                return false;
    return true;
}

u32 revent_at(Str data, usize i)
{
    const u8 *p = reinterpret_cast<const u8 *>(data.data()) + i * sizeof(u32);
    return u32(p[0]) | u32(p[1]) << 8 | u32(p[2]) << 16 | u32(p[3]) << 24;
}

// Err(Invalid) out of a poll is a descriptor the kernel will not watch: one
// that is not open, or one asked about in a direction it has not got, since a
// pipe has only the one. The count is checked here, so it is not that.
R err_poll(Error e)
{
    return e == Error::Invalid ? err_errno(9) : err_os(e); // EBADF
}

// Too many to ask about at once. The bound is the kernel's, because each
// descriptor is armed on its channel for the length of the call.
R err_too_many(usize n)
{
    char buf[24];
    return err_set2("ValueError", "too many descriptors to poll",
                    int_text(buf, sizeof buf, i64(n)));
}

// What an object that is not a descriptor and has no fileno() is.
R err_fileno()
{
    return err_set("TypeError", "argument must be an int, or have a fileno() method.");
}

// The deadline a request is entered with; a negative one waits for ever.
i64 poll_deadline(i64 ms, bool forever)
{
    return forever ? -1 : i64(proc_now()) + ms;
}

// The timeout of a request that has already been interrupted once: what is
// left of it, so a handler that returns does not start the wait again.
u32 poll_left(const ContObj *k)
{
    if (k->x[0] < 0)
        return SYS_POLL_FOREVER;
    i64 left = k->x[0] - i64(proc_now());
    return left <= 0 ? 0 : u32(left);
}

// `v` as a timeout in milliseconds, rounded up as CPython's is: a wait is at
// least as long as it was asked for. None waits for ever, and so does a
// negative number given to poll(), which select() refuses instead. `seconds`
// is what select() counts in and poll() does not.
bool timeout_ms(Value v, bool seconds, Str who, i64 &ms, bool &forever)
{
    forever = is_none(v) || v.is_nil();
    ms      = 0;
    if (forever)
        return true;
    f64 t = 0;
    if (!as_number(v, t)) {
        err_set2("TypeError", "timeout must be a number or None", who);
        return false;
    }
    if (t < 0) {
        if (seconds) {
            err_set("ValueError", "timeout must be non-negative");
            return false;
        }
        forever = true;
        return true;
    }
    if (seconds)
        t *= 1000;
    // The comparison comes before the conversion: a double past the range is
    // undefined as an integer, not merely wrong.
    if (!(t <= 0x7fffffffp0)) {
        err_set("OverflowError", "timeout is too large");
        return false;
    }
    i64 down = i64(t);
    ms       = t > f64(down) ? down + 1 : down;
    return true;
}

// ------------------------------------------------------------------- poll()

struct PollObj : Obj {
    Vec<u32> fds; // in registration order, which is the order CPython's dict
    Vec<u32> evs; // was in, and so the order poll() answers in
};

extern const Type poll_type;

PollObj *poll_of(Value v)
{
    return static_cast<PollObj *>(v.obj());
}

void poll_fini(Obj *o)
{
    static_cast<PollObj *>(o)->fds.~Vec();
    static_cast<PollObj *>(o)->evs.~Vec();
}

R poll_repr(Value, String &out)
{
    return out.append("<select.poll object>") ? R::Ok : oom();
}

// Where `fd` is registered, or the size when it is not.
usize poll_find(const PollObj *p, u32 fd)
{
    usize i = 0;
    while (i < p->fds.size() && p->fds[i] != fd)
        i++;
    return i;
}

enum : u32 { OP_REG, OP_MOD, OP_UNREG };

R poll_apply(Value self, u32 op, i64 fd, u32 mask, Value &out)
{
    if (fd < 0)
        return err_set("ValueError", "file descriptor cannot be a negative integer");
    PollObj *p = poll_of(self);
    usize at   = poll_find(p, u32(fd));
    char buf[24];
    Str text = int_text(buf, sizeof buf, fd);
    if (op == OP_UNREG) {
        if (at == p->fds.size())
            return err_set2("KeyError", "descriptor is not registered", text);
        p->fds.erase(at);
        p->evs.erase(at);
        out = value_none();
        return R::Ok;
    }
    if (op == OP_MOD && at == p->fds.size())
        return err_errno(2, Value()); // ENOENT, as CPython's modify raises
    if (at < p->fds.size()) {
        p->evs[at] = mask;
    } else {
        if (!p->fds.push(u32(fd)))
            return oom();
        if (!p->evs.push(mask)) {
            p->fds.pop();
            return oom();
        }
    }
    out = value_none();
    return R::Ok;
}

// s[0] the poll object, s[1] the descriptor as given; i the op, x[0] the mask
// and x[1] whether fileno() has been called.
R fdop_step(ContObj *k, Value in)
{
    if (!k->x[1]) {
        k->x[1]     = 1;
        k->catching = CATCH_ATTR;
        return cont_method(k, k->s[1], "fileno");
    }
    k->catching = CATCH_NONE;
    i64 fd      = 0;
    if (in.is_nil() || !as_int_arg(in, fd))
        return err_fileno();
    Value out;
    R r = poll_apply(k->s[0], k->i, fd, u32(k->x[0]), out);
    return r == R::Ok ? cont_done(k, out) : r;
}

// The descriptor an argument names: a number as it stands, or fileno() called
// for it, which is a continuation because a native cannot call Python.
R fdop_start(Value self, Value obj, u32 op, u32 mask, Value &out)
{
    i64 fd = 0;
    if (as_int_arg(obj, fd))
        return poll_apply(self, op, fd, mask, out);
    Root rs{ self }, ro{ obj };
    Root kv{ cont_new(fdop_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = rs.v;
    k->s[1]    = ro.v;
    k->i       = op;
    k->x[0]    = mask;
    out        = kv.v;
    return R::Ok;
}

PollObj *self_poll(const CallArgs &a, Str who)
{
    Value s = method_self(a.args[0]);
    if (!s.is_obj() || s.obj()->type != &poll_type)
        return err_set2("TypeError", "descriptor requires a select.poll object", who), nullptr;
    return poll_of(s);
}

R pm_register(const CallArgs &a, Value &out)
{
    if (!self_poll(a, "register") || !meth_args(a, "register", 1, 2))
        return R::Err;
    i64 mask = POLLIN | POLLPRI | POLLOUT;
    if (a.nargs == 3 && !as_index(a.args[2], mask))
        return err_set("TypeError", "eventmask must be an integer");
    return fdop_start(method_self(a.args[0]), a.args[1], OP_REG, u32(mask), out);
}

R pm_modify(const CallArgs &a, Value &out)
{
    if (!self_poll(a, "modify") || !meth_args(a, "modify", 2, 2))
        return R::Err;
    i64 mask = 0;
    if (!as_index(a.args[2], mask))
        return err_set("TypeError", "eventmask must be an integer");
    return fdop_start(method_self(a.args[0]), a.args[1], OP_MOD, u32(mask), out);
}

R pm_unregister(const CallArgs &a, Value &out)
{
    if (!self_poll(a, "unregister") || !meth_args(a, "unregister", 1, 1))
        return R::Err;
    return fdop_start(method_self(a.args[0]), a.args[1], OP_UNREG, 0, out);
}

// s[0] the poll object; x[0] the deadline. The request is built again on every
// entry, which is what sys_turn wants and what takes the time already spent
// off an interrupted wait.
R poll_step(ContObj *k, Value)
{
    PollObj *p = poll_of(k->s[0]);
    if (p->fds.size() > SYS_POLL_MAX)
        return err_too_many(p->fds.size());
    String payload;
    for (usize i = 0; i < p->fds.size(); i++)
        if (!pair_push(payload, p->fds[i], events_out(p->evs[i])))
            return oom();
    SysReq q;
    q.op   = SysOp::Poll;
    q.max  = poll_left(k);
    q.data = payload.str();
    R r;
    if (!sys_turn(k, q, r))
        return r == R::Err && vm_sys_answer().err == Error::Invalid ? err_poll(Error::Invalid) : r;
    SysAns &a  = vm_sys_answer();
    ListObj *l = list_new();
    if (!l)
        return R::Err;
    Root rl{ obj_value(l) };
    p = poll_of(k->s[0]);
    for (usize i = 0; i < p->fds.size() && i * sizeof(u32) < a.data.size(); i++) {
        u32 got = events_in(revent_at(a.data.str(), i));
        if (!got)
            continue;
        TupleObj *t = tuple_new(2);
        if (!t)
            return R::Err;
        t->items()[0] = Value::of_int(i32(p->fds[i]));
        t->items()[1] = Value::of_int(i32(got));
        Root rt{ obj_value(t) }; // list_push may collect before it takes it
        if (!list_push(list_of(rl.v), rt.v))
            return oom();
        p = poll_of(k->s[0]);
    }
    return cont_done(k, rl.v);
}

R pm_poll(const CallArgs &a, Value &out)
{
    if (!self_poll(a, "poll") || !meth_args(a, "poll", 0, 1))
        return R::Err;
    i64 ms      = 0;
    bool always = true;
    if (!timeout_ms(a.nargs == 2 ? a.args[1] : Value(), false, "poll", ms, always))
        return R::Err;
    Root rs{ method_self(a.args[0]) };
    Root kv{ cont_new(poll_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = rs.v;
    k->x[0]    = poll_deadline(ms, always);
    out        = kv.v;
    return R::Ok;
}

constexpr Method POLL_METHODS[] = {
    { "register", pm_register },
    { "modify", pm_modify },
    { "unregister", pm_unregister },
    { "poll", pm_poll },
};

constexpr Type poll_type{ .name = "select.poll", .fini = poll_fini, .repr = poll_repr };

// select.poll is a function and not the type, as CPython's is: the type itself
// refuses to be called, which is what check_disallow_instantiation asks.
R s_poll(const CallArgs &a, Value &out)
{
    if (!args_only(a, "poll", 0, 0))
        return R::Err;
    PollObj *p = static_cast<PollObj *>(obj_alloc(&poll_type, sizeof(PollObj)));
    if (!p)
        return oom();
    new (&p->fds) Vec<u32>();
    new (&p->evs) Vec<u32>();
    out = obj_value(p);
    return R::Ok;
}

// ----------------------------------------------------------------- select()

// s[0] the three lists as given, s[1] every object in them in turn, s[2] their
// descriptors beside them, s[3] the one whose fileno() is out; i is which list
// is being walked and 3 for the wait, j the index into that list. x[0] the
// deadline, x[1] and x[2] where the read list's entries and the write list's
// end, x[3] whether fileno() is out.
//
// The three lists are walked live and their size read again at every step,
// because a fileno() may shorten the list it is in -- which is what CPython's
// seq2set does, and what test_select_mutated asks for.
bool select_take(ContObj *k, Value o, i64 fd)
{
    Root ro{ o };
    if (!list_push(list_of(k->s[1]), ro.v))
        return false;
    return list_push(list_of(k->s[2]), Value::of_int(i32(fd)));
}

R select_step(ContObj *k, Value in)
{
    u32 phase = k->i & ~SYS_TURN_BITS;
    while (phase < 3) {
        if (k->x[3]) {
            // Back from fileno(), which a missing attribute answered Nil to.
            k->x[3]     = 0;
            k->catching = CATCH_NONE;
            i64 n       = 0;
            if (in.is_nil() || !as_int_arg(in, n))
                return err_fileno();
            if (n < 0)
                return err_set("ValueError", "file descriptor cannot be a negative integer");
            if (!select_take(k, k->s[3], n))
                return oom();
            k->s[3] = Value();
        }
        ListObj *l = list_of(static_cast<TupleObj *>(k->s[0].obj())->items()[phase]);
        if (k->j >= l->items.size()) {
            // Where this list's entries end, for the answer to be sorted into.
            if (phase < 2)
                k->x[1 + phase] = i64(list_of(k->s[1])->items.size());
            k->i = ++phase;
            k->j = 0;
            continue;
        }
        Value o = l->items[k->j++];
        i64 n   = 0;
        if (as_int_arg(o, n)) {
            if (n < 0)
                return err_set("ValueError", "file descriptor cannot be a negative integer");
            if (!select_take(k, o, n))
                return oom();
            continue;
        }
        k->s[3]     = o;
        k->x[3]     = 1;
        k->catching = CATCH_ATTR;
        return cont_method(k, o, "fileno");
    }

    // One entry per descriptor, however many lists named it: the kernel holds
    // each for the length of the call, so naming one twice is Err(Busy).
    ListObj *fds = list_of(k->s[2]);
    usize nr     = usize(k->x[1]);
    usize nw     = usize(k->x[2]) - nr;
    Vec<u32> one, mask;
    for (usize i = 0; i < fds->items.size(); i++) {
        u32 fd = u32(fds->items[i].as_int());
        u32 want =
            i < nr ? SYS_POLL_IN : (i < nr + nw ? SYS_POLL_OUT : 0); // the third list asks nothing
        usize at = 0;
        while (at < one.size() && one[at] != fd)
            at++;
        if (at < one.size()) {
            mask[at] |= want;
            continue;
        }
        if (!one.push(fd) || !mask.push(want))
            return oom();
    }
    if (one.size() > SYS_POLL_MAX)
        return err_too_many(one.size());
    String payload;
    for (usize i = 0; i < one.size(); i++)
        if (!pair_push(payload, one[i], mask[i]))
            return oom();
    SysReq q;
    q.op   = SysOp::Poll;
    q.max  = poll_left(k);
    q.data = payload.str();
    R r;
    if (!sys_turn(k, q, r))
        return r == R::Err && vm_sys_answer().err == Error::Invalid ? err_poll(Error::Invalid) : r;
    Str got = vm_sys_answer().data.str();

    // Each list keeps the objects it was given, in the order it gave them.
    TupleObj *t = tuple_new(3);
    if (!t)
        return oom();
    Root rt{ obj_value(t) };
    for (u32 n = 0; n < 3; n++) {
        ListObj *l = list_new();
        if (!l)
            return R::Err;
        static_cast<TupleObj *>(rt.v.obj())->items()[n] = obj_value(l);
    }
    ListObj *all = list_of(k->s[1]);
    fds          = list_of(k->s[2]);
    u32 ready[3] = { SYS_POLL_IN | SYS_POLL_HUP, SYS_POLL_OUT | SYS_POLL_HUP, 0 };
    for (usize i = 0; i < fds->items.size() && i < all->items.size(); i++) {
        u32 fd   = u32(fds->items[i].as_int());
        usize at = 0;
        while (at < one.size() && one[at] != fd)
            at++;
        if (at >= one.size() || at * sizeof(u32) >= got.size())
            continue;
        u32 which = i < nr ? 0 : (i < nr + nw ? 1 : 2);
        if (!(revent_at(got, at) & ready[which]))
            continue;
        Value o    = list_of(k->s[1])->items[i];
        Value dest = static_cast<TupleObj *>(rt.v.obj())->items()[which];
        if (!list_push(list_of(dest), o))
            return oom();
        fds = list_of(k->s[2]);
    }
    return cont_done(k, rt.v);
}

R s_select(const CallArgs &a, Value &out)
{
    if (!args_only(a, "select", 3, 4))
        return R::Err;
    i64 ms      = 0;
    bool always = true;
    if (!timeout_ms(a.nargs == 4 ? a.args[3] : Value(), true, "select", ms, always))
        return R::Err;
    TupleObj *lists = tuple_new(3);
    if (!lists)
        return oom();
    Root rl{ obj_value(lists) };
    ListObj *all = list_new();
    if (!all)
        return oom();
    Root ra{ obj_value(all) };
    for (u32 n = 0; n < 3; n++) {
        // A list is kept as it stands and anything else is copied, which is
        // PySequence_Fast's rule and is what makes a mutation visible.
        Value l = a.args[n];
        if (!is_list(l)) {
            ListObj *made = py_list_of(l);
            if (!made)
                return R::Err;
            l = obj_value(made);
        }
        static_cast<TupleObj *>(rl.v.obj())->items()[n] = l;
    }
    ListObj *fds = list_new();
    if (!fds)
        return oom();
    Root rf{ obj_value(fds) };
    Root kv{ cont_new(select_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = rl.v;
    k->s[1]    = ra.v;
    k->s[2]    = rf.v;
    k->x[0]    = poll_deadline(ms, always);
    out        = kv.v;
    return R::Ok;
}

constexpr ModDef DEFS[] = {
    { "select", s_select },
    { "poll", s_poll },
};

} // namespace

bool select_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    DictObj *d = static_cast<DictObj *>(rd.v.obj());
    // The type goes in first and without a constructor; DEFS then puts the
    // function that makes one over the name it left behind.
    if (!method_install(&poll_type, POLL_METHODS) || !mod_type(d, &poll_type))
        return false;
    if (!mod_defs(d, DEFS) || !mod_int(d, "PIPE_BUF", 4096))
        return false;
    constexpr struct {
        Str name;
        i64 v;
    } BITS[] = {
        { "POLLIN", POLLIN },   { "POLLPRI", POLLPRI }, { "POLLOUT", POLLOUT },
        { "POLLERR", POLLERR }, { "POLLHUP", POLLHUP }, { "POLLNVAL", POLLNVAL },
    };
    for (const auto &b : BITS)
        if (!mod_int(d, b.name, b.v))
            return false;
    // select.error is OSError, as it has been since 3.3.
    Value e;
    StrObj *n = str_intern("OSError");
    if (!n || dict_get(builtins_dict(), obj_value(n), e) != R::Ok)
        return false;
    return mod_put(d, "error", e);
}
