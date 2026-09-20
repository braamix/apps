// `_lsprof`: the profiler cProfile stands on, driven from the VM directly.
//
// The whole point of it over `profile.py` is that no Python runs per event:
// the interpreter calls in here at every call and return, and what it keeps
// is a table of entries and a stack of the calls in flight. `getstats()`
// hands that table over as the objects `profiling.tracing` reads.
//
// The timer is `proc_now()`, which counts whole milliseconds, so a profile
// here has a much coarser grain than CPython's -- and under the test harness
// the clock is frozen, so every time is zero and only the counts mean
// anything. A timer of the program's own would have to be a Python call per
// event, which is what this module exists to avoid, so it is refused.
#include "bigint.h"
#include "builtin.h"
#include "code.h"
#include "func.h"
#include "gc.h"
#include "intern.h"
#include "kernel/fmt.h"
#include "kernel/vec.h"
#include "method.h"
#include "module.h"
#include "obj.h"
#include "ops.h"
#include "proc/rt.h"
#include "type.h"
#include "vm.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

// What one callable has cost. `key` is a code object for a Python function
// and the callable itself for a builtin, which is what `label()` in
// profiling.tracing expects to see.
struct Sub {
    Value key;
    i64 callcount    = 0;
    i64 reccallcount = 0;
    i64 totaltime    = 0;
    i64 inlinetime   = 0;
};

struct Entry {
    Value key;
    i64 callcount    = 0;
    i64 reccallcount = 0;
    i64 totaltime    = 0;
    i64 inlinetime   = 0;
    u32 recursion    = 0; // activations of this one on the stack now
    Vec<Sub> calls;       // what it called, when subcalls are recorded
};

// One call in flight: when it started, and how much of that belongs to the
// calls it made rather than to itself.
struct Ctx {
    u32 entry;
    i64 t0;
    i64 child;
};

struct ProfObj : Obj {
    Vec<Entry> entries;
    Vec<Ctx> stack;
    bool running;
    bool subcalls;
    bool builtins;
};

extern const Type prof_type;

// The one profiler the VM is told about. Only one can run: the hook is a
// single function pointer, which is the price of not making a call.
ProfObj *active;

ProfObj *prof_of(Value v)
{
    return static_cast<ProfObj *>(v.obj());
}

void prof_trace(Obj *o)
{
    ProfObj *p = static_cast<ProfObj *>(o);
    for (usize i = 0; i < p->entries.size(); i++) {
        gc_mark(p->entries[i].key);
        for (usize j = 0; j < p->entries[i].calls.size(); j++)
            gc_mark(p->entries[i].calls[j].key);
    }
}

void prof_fini(Obj *o)
{
    ProfObj *p = static_cast<ProfObj *>(o);
    if (active == p)
        active = nullptr;
    for (usize i = 0; i < p->entries.size(); i++)
        p->entries[i].calls.~Vec();
    p->entries.~Vec();
    p->stack.~Vec();
}

R prof_repr(Value, String &out)
{
    return out.append("<_lsprof.Profiler object>") ? R::Ok : oom();
}

// The profiler is a root of its own while it is running: the VM holds it
// through a bare pointer and nothing else need name it.
void prof_root()
{
    if (active)
        gc_mark(obj_value(active));
}

// ------------------------------------------------------------ the counting

// Whole milliseconds. The browser's clock, and frozen under the harness.
i64 now_ms()
{
    return i64(proc_now());
}

// `key`'s entry, made on first sight. The table is a vector and the lookup
// is linear: a profile has tens of entries, not thousands, and a dict would
// cost a hash of a code object per event.
u32 entry_for(ProfObj *p, Value key)
{
    for (u32 i = 0; i < p->entries.size(); i++)
        if (p->entries[i].key == key)
            return i;
    Entry e;
    e.key = key;
    if (!p->entries.push(static_cast<Entry &&>(e)))
        return ~0u;
    return u32(p->entries.size() - 1);
}

// The same, among what an entry called.
Sub *sub_for(Entry &e, Value key)
{
    for (usize i = 0; i < e.calls.size(); i++)
        if (e.calls[i].key == key)
            return &e.calls[i];
    Sub s;
    s.key = key;
    if (!e.calls.push(s))
        return nullptr;
    return &e.calls[e.calls.size() - 1];
}

// A builtin's key is the native itself and not the bound method a call made
// of it: `x.append` is a fresh object each time, and the native under it is
// not. Nothing here allocates, which is what lets an event run in the middle
// of an instruction.
Value native_key(Value v)
{
    return is_method(v) ? static_cast<MethodObj *>(v.obj())->fn : v;
}

// What CPython's normalizeUserObj writes down for a builtin, built only when
// getstats() asks: a method of a type, a function of a module, or a bare
// name. profiling.tracing takes a str for a builtin and a code object for
// anything written in Python.
Value native_label(Value v)
{
    NativeObj *n = static_cast<NativeObj *>(v.obj());
    Buf<128> b;
    if (is_type(n->owner)) {
        b.put("<method '").put(n->name).put("' of '");
        b.put(type_obj(n->owner)->slots.name).put("' objects>");
    } else if (is_str(n->owner)) {
        b.put("<built-in method ").put(str_of(n->owner)->str()).put('.').put(n->name).put('>');
    } else {
        b.put("<built-in function ").put(n->name).put('>');
    }
    return str_new(b.str());
}

void enter_call(ProfObj *p, Value key)
{
    u32 at = entry_for(p, key);
    if (at == ~0u)
        return; // out of memory: the profile loses this call and no more
    p->entries[at].recursion++;
    Ctx c;
    c.entry = at;
    c.t0    = now_ms();
    c.child = 0;
    if (!p->stack.push(c))
        p->entries[at].recursion--;
}

// One call in flight, accounted for and taken off the stack.
void account(ProfObj *p, Ctx c)
{
    i64 tt  = now_ms() - c.t0;
    i64 it   = tt - c.child;
    Entry &e = p->entries[c.entry];
    e.recursion--;
    e.callcount++;
    e.inlinetime += it;
    if (e.recursion)
        e.reccallcount++;
    else
        e.totaltime += tt;
    if (p->stack.empty())
        return;
    // The caller keeps this call's whole cost as its own child time, and
    // remembers what it spent here.
    Ctx &up = p->stack[p->stack.size() - 1];
    up.child += tt;
    if (!p->subcalls)
        return;
    Sub *s = sub_for(p->entries[up.entry], p->entries[c.entry].key);
    if (!s)
        return;
    s->callcount++;
    s->inlinetime += it;
    if (e.recursion)
        s->reccallcount++;
    else
        s->totaltime += tt;
}

void leave_call(ProfObj *p, Value key)
{
    // A return with nothing on the stack is one whose call was never seen --
    // the frame the profiler was turned on inside. It is dropped.
    if (p->stack.empty())
        return;
    Ctx c = p->stack[p->stack.size() - 1];
    if (p->entries[c.entry].key != key)
        return;
    p->stack.pop();
    account(p, c);
}

// Everything still in flight when the profiler stops, counted where it
// stands: CPython's flush_unmatched, and it is what makes disable() itself
// appear in the stats.
void flush_stack(ProfObj *p)
{
    while (!p->stack.empty()) {
        Ctx c = p->stack[p->stack.size() - 1];
        p->stack.pop();
        account(p, c);
    }
}

// What the VM calls. Nothing here allocates a Python object, so it cannot
// fail in a way the interpreter has to hear about.
void on_event(u32 event, Value what)
{
    ProfObj *p = active;
    if (!p || !p->running)
        return;
    switch (event) {
    case PROF_CALL:
        enter_call(p, what);
        break;
    case PROF_RETURN:
        leave_call(p, what);
        break;
    case PROF_C_CALL:
        if (p->builtins)
            enter_call(p, native_key(what));
        break;
    case PROF_C_RETURN:
        if (p->builtins)
            leave_call(p, native_key(what));
        break;
    default:
        break;
    }
}

// ------------------------------------------------------- what getstats says

// profiler_entry and profiler_subentry, which profiling.tracing reads by
// name. One type for both: a subentry is an entry with no `calls`.
struct StatObj : Obj {
    Value key;
    Value calls; // ListObj of StatObj, or Nil for a subentry
    i64 callcount;
    i64 reccallcount;
    i64 totaltime;
    i64 inlinetime;
};

extern const Type stat_type;

void stat_trace(Obj *o)
{
    gc_mark(static_cast<StatObj *>(o)->key);
    gc_mark(static_cast<StatObj *>(o)->calls);
}

R stat_repr(Value v, String &out)
{
    StatObj *s = static_cast<StatObj *>(v.obj());
    Buf<64> b;
    b.put(s->calls.is_nil() ? "<profiler_subentry " : "<profiler_entry ");
    char t[24];
    b.put(int_text(t, sizeof t, s->callcount)).put(" calls>");
    return out.append(b.str()) ? R::Ok : oom();
}

// Seconds, which is what pstats prints. The clock counts milliseconds.
Value seconds(i64 ms)
{
    return float_new(f64(ms) / 1000.0);
}

R stat_getattr(Value v, StrObj *name, Value &out)
{
    StatObj *s = static_cast<StatObj *>(v.obj());
    Str n      = name->str();
    if (n == "code")
        out = s->key;
    else if (n == "callcount")
        out = int_from_i64(s->callcount);
    else if (n == "reccallcount")
        out = int_from_i64(s->reccallcount);
    else if (n == "totaltime")
        out = seconds(s->totaltime);
    else if (n == "inlinetime")
        out = seconds(s->inlinetime);
    else if (n == "calls")
        out = s->calls.is_nil() ? value_none() : s->calls;
    else
        return R::NotImpl;
    return out.is_nil() ? R::Err : R::Ok;
}

constexpr Type stat_type{ .name    = "_lsprof.profiler_entry",
                          .trace   = stat_trace,
                          .repr    = stat_repr,
                          .getattr = stat_getattr,
                          .final   = true };

Value stat_new(Value key, i64 cc, i64 rc, i64 tt, i64 it, Value calls)
{
    Root rk{ key }, rl{ calls };
    if (is_native(rk.v)) {
        rk = native_label(rk.v);
        if (rk.v.is_nil())
            return Value();
    }
    StatObj *s = static_cast<StatObj *>(obj_alloc(&stat_type, sizeof(StatObj)));
    if (!s)
        return oom(), Value();
    s->key          = rk.v;
    s->calls        = rl.v;
    s->callcount    = cc;
    s->reccallcount = rc;
    s->totaltime    = tt;
    s->inlinetime   = it;
    return obj_value(s);
}

// ------------------------------------------------------------ the methods

ProfObj *self_prof(const CallArgs &a, Str who)
{
    Value s = method_self(a.args[0]);
    if (!s.is_obj() || s.obj()->type != &prof_type)
        return err_set2("TypeError", "descriptor requires a Profiler", who), nullptr;
    return prof_of(s);
}

R pm_enable(const CallArgs &a, Value &out)
{
    ProfObj *p = self_prof(a, "enable");
    if (!p)
        return R::Err;
    static const Str NAMES[] = { "subcalls", "builtins" };
    Value got[2];
    if (!meth_take(a, "enable", NAMES, 0, got))
        return R::Err;
    if (!got[0].is_nil())
        p->subcalls = py_truth(got[0]);
    if (!got[1].is_nil())
        p->builtins = py_truth(got[1]);
    if (active && active != p && active->running)
        return err_set("RuntimeError", "another profiler is already running");
    active     = p;
    p->running = true;
    p->stack.clear();
    vm_set_native_profile(on_event);
    out = value_none();
    return R::Ok;
}

R pm_disable(const CallArgs &a, Value &out)
{
    ProfObj *p = self_prof(a, "disable");
    if (!p || !meth_args(a, "disable", 0, 0))
        return R::Err;
    flush_stack(p);
    p->running = false;
    if (active == p) {
        active = nullptr;
        vm_set_native_profile(nullptr);
    }
    out = value_none();
    return R::Ok;
}

R pm_clear(const CallArgs &a, Value &out)
{
    ProfObj *p = self_prof(a, "clear");
    if (!p || !meth_args(a, "clear", 0, 0))
        return R::Err;
    for (usize i = 0; i < p->entries.size(); i++)
        p->entries[i].calls.clear();
    p->entries.clear();
    p->stack.clear();
    out = value_none();
    return R::Ok;
}

R pm_getstats(const CallArgs &a, Value &out)
{
    ProfObj *p = self_prof(a, "getstats");
    if (!p || !meth_args(a, "getstats", 0, 0))
        return R::Err;
    ListObj *l = list_new();
    if (!l)
        return R::Err;
    Root rl{ obj_value(l) };
    Root rp{ method_self(a.args[0]) };
    for (u32 i = 0; i < prof_of(rp.v)->entries.size(); i++) {
        Root subs;
        if (!prof_of(rp.v)->entries[i].calls.empty()) {
            ListObj *sl = list_new();
            if (!sl)
                return R::Err;
            subs = obj_value(sl);
            for (u32 j = 0; j < prof_of(rp.v)->entries[i].calls.size(); j++) {
                const Sub &s = prof_of(rp.v)->entries[i].calls[j];
                Root one{ stat_new(s.key, s.callcount, s.reccallcount, s.totaltime, s.inlinetime,
                                   Value()) };
                if (one.v.is_nil() || !list_push(list_of(subs.v), one.v))
                    return R::Err;
            }
        }
        const Entry &e = prof_of(rp.v)->entries[i];
        Root one{ stat_new(e.key, e.callcount, e.reccallcount, e.totaltime, e.inlinetime,
                           subs.v.is_nil() ? obj_value(list_new()) : subs.v) };
        if (one.v.is_nil() || !list_push(list_of(rl.v), one.v))
            return R::Err;
    }
    out = rl.v;
    return R::Ok;
}

constexpr Method PROF_METHODS[] = {
    { "enable", pm_enable },
    { "disable", pm_disable },
    { "clear", pm_clear },
    { "getstats", pm_getstats },
};

constexpr Type prof_type{ .name  = "_lsprof.Profiler",
                          .trace = prof_trace,
                          .fini  = prof_fini,
                          .repr  = prof_repr };

R b_profiler(const CallArgs &a, Value &out)
{
    static const Str NAMES[] = { "timer", "timeunit", "subcalls", "builtins" };
    Value got[4];
    if (!func_take(a, "Profiler", NAMES, 0, got))
        return R::Err;
    // A timer of the program's own would be a Python call at every event,
    // which is the one thing this module is for not doing.
    if (!got[0].is_nil() && !is_none(got[0]))
        return err_set("ValueError", "a timer of one's own is not supported here");
    ProfObj *p = static_cast<ProfObj *>(obj_alloc(&prof_type, sizeof(ProfObj)));
    if (!p)
        return oom();
    new (&p->entries) Vec<Entry>();
    new (&p->stack) Vec<Ctx>();
    p->running  = false;
    p->subcalls = got[2].is_nil() || py_truth(got[2]);
    p->builtins = got[3].is_nil() || py_truth(got[3]);
    out         = obj_value(p);
    return R::Ok;
}

} // namespace

bool lsprof_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    gc_root_hook(prof_root);
    DictObj *d = static_cast<DictObj *>(rd.v.obj());
    return method_install(&prof_type, PROF_METHODS) && mod_type(d, &prof_type, b_profiler) &&
           mod_type(d, &stat_type);
}
