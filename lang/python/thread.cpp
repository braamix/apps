// `_thread`: the locks and the thread-local namespace, for a process that has
// one thread.
//
// A Web Worker runs one interpreter and nothing else, so a lock is never
// contended by anyone but its holder. What waiting would mean -- a timeout, or
// a wait for ever -- is answered at once: False, or the deadlock it would be.
// A new thread cannot be started.
#include "builtin.h"
#include "gc.h"
#include "intern.h"
#include "kernel/fmt.h"
#include "method.h"
#include "module.h"
#include "ops.h"
#include "proc/rt.h"
#include "type.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

constexpr f64 TIMEOUT_MAX = 9223372036.0; // PY_TIMEOUT_MAX in seconds

// The one thread's ident.
i64 ident()
{
    return i64(proc_pid());
}

bool no_args(const CallArgs &a, Str who, u32 self)
{
    if (a.nkw) {
        Buf<96> b;
        b.put(who).put("() takes no keyword arguments");
        return err_set("TypeError", b.str()), false;
    }
    if (a.nargs > self) {
        char tmp[24];
        Buf<128> b;
        b.put(who).put("() takes no arguments (");
        b.put(int_text(tmp, sizeof tmp, i64(a.nargs - self))).put(" given)");
        return err_set("TypeError", b.str()), false;
    }
    return true;
}

// ------------------------------------------------------------------ lock

struct LockObj : Obj {
    u32 count; // 0 or 1 for a lock; the depth for an RLock
};

extern const Type lock_type;
extern const Type rlock_type;

LockObj *self_lock(const CallArgs &a, const Type *t, Str who)
{
    Value s = a.nargs ? method_self(a.args[0]) : Value();
    if (!s.is_obj() || s.obj()->type != t) {
        Buf<96> b;
        b.put("descriptor '").put(who).put("' requires a '").put(t->name).put("' object");
        return err_set2("TypeError", b.str(), type_name(s)), nullptr;
    }
    return static_cast<LockObj *>(s.obj());
}

// acquire(blocking=True, timeout=-1): True when the lock is free. A held one
// answers False where CPython would wait, and a wait for ever is a deadlock.
R lock_acquire(const CallArgs &a, Value &out, const Type *t, Str who)
{
    LockObj *l = self_lock(a, t, who);
    if (!l)
        return R::Err;
    static constexpr Str NAMES[] = { "blocking", "timeout" };
    Value got[2];
    if (!meth_take(a, who, NAMES, 2, 0, got))
        return R::Err;
    bool blocking = got[0].is_nil() || py_truth(got[0]);
    f64 timeout   = -1;
    if (!got[1].is_nil()) {
        if (!as_number(got[1], timeout)) {
            Buf<96> b;
            b.put('\'')
                .put(type_name(got[1]))
                .put("' object cannot be interpreted as an integer or float");
            return err_set("TypeError", b.str());
        }
    }
    if (!blocking && timeout != -1)
        return err_set("ValueError", "can't specify a timeout for a non-blocking call");
    if (timeout < 0 && timeout != -1)
        return err_set("ValueError", "timeout value must be a non-negative number");
    if (timeout > TIMEOUT_MAX)
        return err_set("OverflowError", "timestamp out of range for C PyTime_t");
    if (l->count && t == &lock_type) {
        if (blocking && timeout == -1)
            return err_set("RuntimeError",
                           "deadlock: the lock is held and no other thread "
                           "can release it");
        out = value_bool(false);
        return R::Ok;
    }
    l->count++;
    out = value_bool(true);
    return R::Ok;
}

R lock_release(const CallArgs &a, Value &out, const Type *t, Str who)
{
    LockObj *l = self_lock(a, t, who);
    if (!l || !no_args(a, t == &lock_type ? Str("lock.release") : Str("RLock.release"), 1))
        return R::Err;
    if (!l->count)
        return err_set("RuntimeError", t == &lock_type ? "release unlocked lock"
                                                       : "cannot release un-acquired lock");
    l->count--;
    out = value_none();
    return R::Ok;
}

R lock_locked(const CallArgs &a, Value &out, const Type *t, Str who)
{
    LockObj *l = self_lock(a, t, who);
    if (!l || !no_args(a, who, 1))
        return R::Err;
    out = value_bool(l->count != 0);
    return R::Ok;
}

R lock_exit(const CallArgs &a, Value &out, const Type *t)
{
    LockObj *l = self_lock(a, t, "__exit__");
    if (!l)
        return R::Err;
    if (!l->count)
        return err_set("RuntimeError", t == &lock_type ? "release unlocked lock"
                                                       : "cannot release un-acquired lock");
    l->count--;
    out = value_none();
    return R::Ok;
}

R l_acquire(const CallArgs &a, Value &out)
{
    return lock_acquire(a, out, &lock_type, "acquire");
}

R l_release(const CallArgs &a, Value &out)
{
    return lock_release(a, out, &lock_type, "release");
}

R l_locked(const CallArgs &a, Value &out)
{
    return lock_locked(a, out, &lock_type, "locked");
}

R l_exit(const CallArgs &a, Value &out)
{
    return lock_exit(a, out, &lock_type);
}

// A lock forgets its state in a forked child; there is no fork, but the
// method is part of the type.
R l_reinit(const CallArgs &a, Value &out)
{
    LockObj *l = self_lock(a, &lock_type, "_at_fork_reinit");
    if (!l || !no_args(a, "_at_fork_reinit", 1))
        return R::Err;
    l->count = 0;
    out      = value_none();
    return R::Ok;
}

R lock_repr(Value v, String &out, Str name)
{
    LockObj *l = static_cast<LockObj *>(v.obj());
    char tmp[24];
    Buf<160> b;
    b.put('<').put(l->count ? "locked " : "unlocked ").put(name).put(" object");
    if (v.obj()->type == &rlock_type) {
        b.put(" owner=").put(int_text(tmp, sizeof tmp, l->count ? ident() : 0));
        b.put(" count=").put(int_text(tmp, sizeof tmp, i64(l->count)));
    }
    b.put(" at ").put(addr_text(tmp, sizeof tmp, v.obj())).put('>');
    return out.append(b.str()) ? R::Ok : oom();
}

R l_repr(Value v, String &out)
{
    return lock_repr(v, out, "_thread.lock");
}

R r_repr(Value v, String &out)
{
    return lock_repr(v, out, "_thread.RLock");
}

constexpr Method LOCK_METHODS[] = {
    { "acquire", l_acquire },      { "acquire_lock", l_acquire }, { "release", l_release },
    { "release_lock", l_release }, { "locked", l_locked },        { "locked_lock", l_locked },
    { "__enter__", l_acquire },    { "__exit__", l_exit },        { "_at_fork_reinit", l_reinit },
};

constexpr Type lock_type{ .name = "_thread.lock", .repr = l_repr, .final = true };

LockObj *lock_make(const Type *t)
{
    LockObj *l = static_cast<LockObj *>(obj_alloc(t, sizeof(LockObj)));
    if (!l)
        return oom(), nullptr;
    l->count = 0;
    return l;
}

R b_allocate_lock(const CallArgs &a, Value &out)
{
    if (!no_args(a, "_thread.allocate_lock", 0))
        return R::Err;
    out = obj_value(lock_make(&lock_type));
    return out.is_nil() ? R::Err : R::Ok;
}

// The type called: `_thread.lock()`.
R b_lock(const CallArgs &a, Value &out)
{
    if (!no_args(a, "lock", 0))
        return R::Err;
    out = obj_value(lock_make(&lock_type));
    return out.is_nil() ? R::Err : R::Ok;
}

// ------------------------------------------------------------------ RLock

R r_acquire(const CallArgs &a, Value &out)
{
    return lock_acquire(a, out, &rlock_type, "acquire");
}

R r_release(const CallArgs &a, Value &out)
{
    return lock_release(a, out, &rlock_type, "release");
}

R r_locked(const CallArgs &a, Value &out)
{
    return lock_locked(a, out, &rlock_type, "locked");
}

R r_exit(const CallArgs &a, Value &out)
{
    return lock_exit(a, out, &rlock_type);
}

R r_is_owned(const CallArgs &a, Value &out)
{
    return lock_locked(a, out, &rlock_type, "_is_owned");
}

R r_count(const CallArgs &a, Value &out)
{
    LockObj *l = self_lock(a, &rlock_type, "_recursion_count");
    if (!l || !no_args(a, "_recursion_count", 1))
        return R::Err;
    out = int_from_i64(i64(l->count));
    return out.is_nil() ? R::Err : R::Ok;
}

// Condition.wait's pair: give the whole depth up, and take it back.
R r_release_save(const CallArgs &a, Value &out)
{
    LockObj *l = self_lock(a, &rlock_type, "_release_save");
    if (!l || !no_args(a, "_release_save", 1))
        return R::Err;
    if (!l->count)
        return err_set("RuntimeError", "cannot release un-acquired lock");
    Root n{ int_from_i64(i64(l->count)) };
    Root id{ int_from_i64(ident()) };
    if (n.v.is_nil() || id.v.is_nil())
        return R::Err;
    TupleObj *t = tuple_new(2);
    if (!t)
        return oom();
    t->items()[0]                                     = n.v;
    t->items()[1]                                     = id.v;
    self_lock(a, &rlock_type, "_release_save")->count = 0;
    out                                               = obj_value(t);
    return R::Ok;
}

R r_acquire_restore(const CallArgs &a, Value &out)
{
    LockObj *l = self_lock(a, &rlock_type, "_acquire_restore");
    if (!l || !meth_args(a, "_acquire_restore", 1, 1))
        return R::Err;
    i64 count = 0;
    if (!is_tuple(a.args[1]) || static_cast<TupleObj *>(a.args[1].obj())->len != 2 ||
        !as_index(static_cast<TupleObj *>(a.args[1].obj())->items()[0], count))
        return err_set("TypeError", "_acquire_restore() argument 1 must be a (count, owner) tuple");
    self_lock(a, &rlock_type, "_acquire_restore")->count = u32(count);
    out                                                  = value_none();
    return R::Ok;
}

R r_reinit(const CallArgs &a, Value &out)
{
    LockObj *l = self_lock(a, &rlock_type, "_at_fork_reinit");
    if (!l || !no_args(a, "_at_fork_reinit", 1))
        return R::Err;
    l->count = 0;
    out      = value_none();
    return R::Ok;
}

constexpr Method RLOCK_METHODS[] = {
    { "acquire", r_acquire },
    { "release", r_release },
    { "locked", r_locked },
    { "__enter__", r_acquire },
    { "__exit__", r_exit },
    { "_is_owned", r_is_owned },
    { "_recursion_count", r_count },
    { "_release_save", r_release_save },
    { "_acquire_restore", r_acquire_restore },
    { "_at_fork_reinit", r_reinit },
};

constexpr Type rlock_type{ .name = "_thread.RLock", .repr = r_repr };

R b_rlock(const CallArgs &a, Value &out)
{
    (void)a;
    out = obj_value(lock_make(&rlock_type));
    return out.is_nil() ? R::Err : R::Ok;
}

// ------------------------------------------------------------------ _local

// With one thread, a thread-local namespace is a namespace.
struct LocalObj : Obj {
    Value dict;
};

void local_trace(Obj *o)
{
    gc_mark(static_cast<LocalObj *>(o)->dict);
}

DictObj *local_dict(Value v)
{
    return static_cast<DictObj *>(static_cast<LocalObj *>(v.obj())->dict.obj());
}

R local_getattr(Value v, StrObj *name, Value &out)
{
    if (name->str() == "__dict__") {
        out = static_cast<LocalObj *>(v.obj())->dict;
        return R::Ok;
    }
    return dict_get(local_dict(v), obj_value(name), out);
}

R local_setattr(Value v, StrObj *name, Value val)
{
    if (name->str() == "__dict__")
        return err_set("AttributeError",
                       "'_thread._local' object attribute '__dict__' is read-only");
    Root rv{ v }, rx{ val };
    if (rx.v.is_nil()) {
        R r = dict_del(local_dict(rv.v), obj_value(name));
        if (r != R::NotImpl)
            return r;
        Buf<128> b;
        b.put("'_thread._local' object has no attribute '").put(name->str()).put('\'');
        return err_set("AttributeError", b.str());
    }
    return dict_set(local_dict(rv.v), obj_value(name), rx.v);
}

constexpr Type local_type{ .name    = "_thread._local",
                           .trace   = local_trace,
                           .getattr = local_getattr,
                           .setattr = local_setattr };

// A subclass that writes __init__ is made without the arguments, which is
// what lets it take any.
R b_local(const CallArgs &a, Value &out)
{
    if (a.nargs || a.nkw)
        return err_set("TypeError", "Initialization arguments are not supported");
    DictObj *d = dict_new();
    if (!d)
        return oom();
    Root rd{ obj_value(d) };
    LocalObj *o = static_cast<LocalObj *>(obj_alloc(&local_type, sizeof(LocalObj)));
    if (!o)
        return oom();
    o->dict = rd.v;
    out     = obj_value(o);
    return R::Ok;
}

// ------------------------------------------------------------------ module

R b_get_ident(const CallArgs &a, Value &out)
{
    if (!no_args(a, "get_ident", 0))
        return R::Err;
    out = int_from_i64(ident());
    return out.is_nil() ? R::Err : R::Ok;
}

R b_start_new_thread(const CallArgs &a, Value &out)
{
    (void)a;
    (void)out;
    return err_set("RuntimeError", "can't start new thread");
}

// The number of threads besides the main one.
R b_count(const CallArgs &a, Value &out)
{
    if (!no_args(a, "_count", 0))
        return R::Err;
    out = Value::of_int(0);
    return R::Ok;
}

R b_stack_size(const CallArgs &a, Value &out)
{
    if (!args_only(a, "stack_size", 0, 1))
        return R::Err;
    i64 n = 0;
    if (a.nargs && !as_index(a.args[0], n))
        return err_set2("TypeError", "object cannot be interpreted as an integer",
                        type_name(a.args[0]));
    if (n != 0 && n < 53248)
        return err_set("ValueError", "size must be at least 53248 bytes");
    if (n != 0)
        return err_set("ValueError", "setting stack size not supported");
    out = Value::of_int(0);
    return R::Ok;
}

R b_interrupt_main(const CallArgs &a, Value &out)
{
    (void)out;
    (void)a;
    return err_set("KeyboardInterrupt", "");
}

constexpr ModDef THREAD_DEFS[] = {
    { "allocate_lock", b_allocate_lock },
    { "allocate", b_allocate_lock },
    { "get_ident", b_get_ident },
    { "get_native_id", b_get_ident },
    { "_get_main_thread_ident", b_get_ident },
    { "start_new_thread", b_start_new_thread },
    { "start_new", b_start_new_thread },
    { "start_joinable_thread", b_start_new_thread },
    { "_count", b_count },
    { "stack_size", b_stack_size },
    { "interrupt_main", b_interrupt_main },
};

} // namespace

bool thread_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    if (!method_install(&lock_type, LOCK_METHODS) || !method_install(&rlock_type, RLOCK_METHODS))
        return false;
    DictObj *d = static_cast<DictObj *>(rd.v.obj());
    if (!mod_type(d, &lock_type, b_lock) || !mod_type(d, &rlock_type, b_rlock) ||
        !mod_type(d, &local_type, b_local) || !mod_defs(d, THREAD_DEFS) ||
        !mod_float(d, "TIMEOUT_MAX", TIMEOUT_MAX))
        return false;
    Root lk{ type_wrap(&lock_type) };
    Value err;
    StrObj *n = str_intern("RuntimeError");
    if (lk.v.is_nil() || !n || dict_get(builtins_dict(), obj_value(n), err) != R::Ok)
        return false;
    return mod_put(d, "LockType", lk.v) && mod_put(d, "error", err);
}
