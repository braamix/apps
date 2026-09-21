// `_tracemalloc`: the tables tracemalloc.py stands on, and the two calls the
// allocator makes into them. See tracemalloc.h for what is traced and why.
#include "tracemalloc.h"

#include "code.h"
#include "frame.h"
#include "func.h"
#include "gc.h"
#include "kernel/alloc.h"
#include "kernel/hash.h"
#include "kernel/string.h"
#include "kernel/vec.h"
#include "module.h"
#include "obj.h"
#include "reduce.h"
#include "vm.h"

bool tm_on;
bool tm_rec;

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

// What a frame whose code object has no filename is recorded as, and the
// whole of the traceback of an allocation made with no frame running.
constexpr Str UNKNOWN = "<unknown>";

// One traced object: the address as a word, the traceback it was made from,
// and what it holds.
struct Ent {
    u32 key;
    u32 tb;
    u32 size;
};

// An interned traceback, read out of the bytes its table is keyed by: the
// real depth first, then a (filename, line) pair per frame kept.
struct TbView {
    const u32 *w;
    u32 nframe;

    u32 total() const { return w[0]; }

    u32 file(u32 i) const { return w[1 + 2 * i]; }

    u32 line(u32 i) const { return w[2 + 2 * i]; }
};

// A Vec and a String have destructors, so the state lives in a heap block: a
// namespace-scope global here must be trivially destructible.
struct Tm {
    Vec<String> names;         // the filenames, one block each so a Str keeps
    HashMap<Str, u32> name_at; // pointing at it while the Vec grows

    Vec<String> tbs; // the interned tracebacks, as TbView reads them
    HashMap<Str, u32> tb_at;

    Vec<Ent> traces;      // every traced object, in no order
    HashMap<u32, u32> at; // its address to its place in `traces`
    String scratch;       // the traceback being captured

    usize traced = 0;
    usize peak   = 0;
    u32 nframe   = 1;
};

Tm *tm;

// No room for another one, which drops the trace and not the allocation.
constexpr u32 NO_ID = ~0u;

// `s` as an index into the filename table, interning it.
u32 name_id(Str s)
{
    if (u32 *p = tm->name_at.find(s))
        return *p;
    String copy;
    if (!copy.append(s) || !tm->names.push(static_cast<String &&>(copy)))
        return NO_ID;
    // Keyed on the copy, whose bytes stay put: String's move takes the block.
    if (!tm->name_at.insert(tm->names.back().str(), u32(tm->names.size() - 1))) {
        tm->names.pop();
        return NO_ID;
    }
    return u32(tm->names.size() - 1);
}

// The frames the VM is in now, as an interned traceback. NO_ID when there is
// no room for one.
u32 capture()
{
    String &s = tm->scratch;
    s.clear();
    u32 total = 0, kept = 0;
    if (!s.append(Str(reinterpret_cast<const char *>(&total), 4)))
        return NO_ID;
    for (Value f = vm_frame(); !f.is_nil(); f = frame_of(f)->back) {
        if (kept < tm->nframe) {
            FrameObj *fr = frame_of(f);
            CodeObj *c   = code_of(fr->code);
            u32 pair[2]  = { is_str(c->filename) ? name_id(str_of(c->filename)->str())
                                                 : name_id(UNKNOWN),
                             code_line(c, fr->pc ? fr->pc - 1 : 0) };
            if (pair[0] == NO_ID ||
                !s.append(Str(reinterpret_cast<const char *>(pair), sizeof pair)))
                return NO_ID;
            kept++;
        }
        if (total < TM_MAX_NFRAME)
            total++;
    }
    // No frame at all -- an object made before the program started, or by the
    // interpreter itself -- is CPython's empty traceback: one unknown frame.
    if (kept == 0) {
        u32 pair[2] = { name_id(UNKNOWN), 0 };
        if (pair[0] == NO_ID || !s.append(Str(reinterpret_cast<const char *>(pair), sizeof pair)))
            return NO_ID;
        total = 1;
    }
    *reinterpret_cast<u32 *>(s.data()) = total;

    if (u32 *p = tm->tb_at.find(s.str()))
        return *p;
    String copy;
    if (!copy.append(s.str()) || !tm->tbs.push(static_cast<String &&>(copy)))
        return NO_ID;
    if (!tm->tb_at.insert(tm->tbs.back().str(), u32(tm->tbs.size() - 1))) {
        tm->tbs.pop();
        return NO_ID;
    }
    return u32(tm->tbs.size() - 1);
}

TbView tb_view(u32 id)
{
    const String &s = tm->tbs[id];
    return TbView{ reinterpret_cast<const u32 *>(s.data()), u32((s.size() / 4 - 1) / 2) };
}

void tm_clear()
{
    tm->names.clear();
    tm->name_at.clear();
    tm->tbs.clear();
    tm->tb_at.clear();
    tm->traces.clear();
    tm->at.clear();
    tm->traced = 0;
    tm->peak   = 0;
}

// ---------------------------------------------------------- the module

// A str per filename and a frames tuple per traceback, so two traces made at
// the same place answer with the same objects, as CPython's intern tables do.
// Both are lists indexed by id, filled as the ids turn up.
Value name_obj(Value cache, u32 id)
{
    ListObj *l = list_of(cache);
    while (l->items.size() <= id)
        if (!list_push(l, Value()))
            return oom(), Value();
    if (!l->items[id].is_nil())
        return l->items[id];
    Value v = str_new(tm->names[id].str());
    if (v.is_nil())
        return Value();
    l->items[id] = v;
    return v;
}

// (filename, lineno) per frame, most recent first, as a tuple.
Value frames_obj(Value names, Value cache, u32 tb)
{
    ListObj *l = list_of(cache);
    while (l->items.size() <= tb)
        if (!list_push(l, Value()))
            return oom(), Value();
    if (!l->items[tb].is_nil())
        return l->items[tb];

    TbView v    = tb_view(tb);
    TupleObj *t = tuple_new(v.nframe);
    if (!t)
        return Value();
    Root rt{ obj_value(t) };
    for (u32 i = 0; i < v.nframe; i++) {
        Root rn{ name_obj(names, tb_view(tb).file(i)) };
        if (rn.v.is_nil())
            return Value();
        Value pair = tuple_of(rn.v, Value::of_int(i32(tb_view(tb).line(i))));
        if (pair.is_nil())
            return Value();
        static_cast<TupleObj *>(rt.v.obj())->items()[i] = pair;
    }
    list_of(cache)->items[tb] = rt.v;
    return rt.v;
}

R t_is_tracing(const CallArgs &a, Value &out)
{
    if (!args_only(a, "is_tracing", 0, 0))
        return R::Err;
    out = value_bool(tm_on);
    return R::Ok;
}

R t_start(const CallArgs &a, Value &out)
{
    if (!args_only(a, "start", 0, 1))
        return R::Err;
    i64 n = 1;
    if (a.nargs == 1 && !as_index(a.args[0], n))
        return err_set("TypeError", "an integer is required");
    if (n < 1 || n > i64(TM_MAX_NFRAME))
        return err_set("ValueError", "the number of frames must be in range [1; 65535]");
    if (!tm_start(u32(n)))
        return oom();
    out = value_none();
    return R::Ok;
}

R t_stop(const CallArgs &a, Value &out)
{
    if (!args_only(a, "stop", 0, 0))
        return R::Err;
    if (tm) {
        tm_on = tm_rec = false;
        tm_clear();
    }
    out = value_none();
    return R::Ok;
}

R t_clear_traces(const CallArgs &a, Value &out)
{
    if (!args_only(a, "clear_traces", 0, 0))
        return R::Err;
    if (tm_on)
        tm_clear();
    out = value_none();
    return R::Ok;
}

R t_get_traceback_limit(const CallArgs &a, Value &out)
{
    if (!args_only(a, "get_traceback_limit", 0, 0))
        return R::Err;
    out = Value::of_int(i32(tm ? tm->nframe : 1));
    return R::Ok;
}

R t_get_traced_memory(const CallArgs &a, Value &out)
{
    if (!args_only(a, "get_traced_memory", 0, 0))
        return R::Err;
    Root cur{ int_from_i64(i64(tm ? tm->traced : 0)) };
    Root peak{ int_from_i64(i64(tm ? tm->peak : 0)) };
    if (cur.v.is_nil() || peak.v.is_nil())
        return R::Err;
    out = tuple_of(cur.v, peak.v);
    return out.is_nil() ? R::Err : R::Ok;
}

R t_reset_peak(const CallArgs &a, Value &out)
{
    if (!args_only(a, "reset_peak", 0, 0))
        return R::Err;
    if (tm_on)
        tm->peak = tm->traced;
    out = value_none();
    return R::Ok;
}

// An estimate: the blocks the three tables hold. HashMap does not report the
// size of a slot, so a slot is counted as its key, its value and its state.
R t_get_tracemalloc_memory(const CallArgs &a, Value &out)
{
    if (!args_only(a, "get_tracemalloc_memory", 0, 0))
        return R::Err;
    usize n = 0;
    if (tm) {
        for (const String &s : tm->names)
            n += s.size();
        for (const String &s : tm->tbs)
            n += s.size();
        n += tm->names.capacity() * sizeof(String) + tm->tbs.capacity() * sizeof(String);
        n += tm->traces.capacity() * sizeof(Ent);
        n += (tm->name_at.capacity() + tm->tb_at.capacity()) * (sizeof(Str) + 8);
        n += tm->at.capacity() * 12;
    }
    out = int_from_i64(i64(n));
    return out.is_nil() ? R::Err : R::Ok;
}

R t_get_object_traceback(const CallArgs &a, Value &out)
{
    if (!args_only(a, "_get_object_traceback", 1, 1))
        return R::Err;
    out = value_none();
    if (!tm_on || !a.args[0].is_obj())
        return R::Ok;
    u32 *p = tm->at.find(a.args[0].w);
    if (!p)
        return R::Ok;
    u32 tb = tm->traces[*p].tb;

    ListObj *names = list_new();
    if (!names)
        return oom();
    Root rn{ obj_value(names) };
    ListObj *tbs = list_new();
    if (!tbs)
        return oom();
    Root rt{ obj_value(tbs) };
    // The build allocates, and an allocation of tracemalloc's own is not one
    // of the program's: recording it would grow the table being read.
    tm_rec = false;
    out    = frames_obj(rn.v, rt.v, tb);
    tm_rec = tm_on;
    return out.is_nil() ? R::Err : R::Ok;
}

R t_get_traces(const CallArgs &a, Value &out)
{
    if (!args_only(a, "_get_traces", 0, 0))
        return R::Err;
    ListObj *l = list_new();
    if (!l)
        return oom();
    Root rl{ obj_value(l) };
    if (!tm_on) {
        out = rl.v;
        return R::Ok;
    }

    // A copy first: the build allocates, and a collection in the middle of it
    // would move the entries under the walk.
    Vec<Ent> snap;
    if (!snap.reserve(tm->traces.size()))
        return oom();
    for (const Ent &e : tm->traces)
        snap.push(e);

    ListObj *names = list_new();
    if (!names)
        return oom();
    Root rn{ obj_value(names) };
    ListObj *tbs = list_new();
    if (!tbs)
        return oom();
    Root rt{ obj_value(tbs) };

    tm_rec  = false;
    bool ok = true;
    for (usize i = 0; i < snap.size() && ok; i++) {
        Root rf{ frames_obj(rn.v, rt.v, snap[i].tb) };
        Root rs{ int_from_i64(i64(snap[i].size)) };
        if (rf.v.is_nil() || rs.v.is_nil()) {
            ok = false;
            break;
        }
        Root row{ tuple_of(Value::of_int(0), rs.v, rf.v,
                           Value::of_int(i32(tb_view(snap[i].tb).total()))) };
        ok = !row.v.is_nil() && list_push(list_of(rl.v), row.v);
    }
    tm_rec = tm_on;
    if (!ok)
        return R::Err;
    out = rl.v;
    return R::Ok;
}

constexpr ModDef DEFS[] = {
    { "is_tracing", t_is_tracing },
    { "clear_traces", t_clear_traces },
    { "_get_traces", t_get_traces },
    { "_get_object_traceback", t_get_object_traceback },
    { "start", t_start },
    { "stop", t_stop },
    { "get_traceback_limit", t_get_traceback_limit },
    { "get_tracemalloc_memory", t_get_tracemalloc_memory },
    { "get_traced_memory", t_get_traced_memory },
    { "reset_peak", t_reset_peak },
};

} // namespace

// ------------------------------------------------------ the allocator's side

void tm_track(Obj *o, usize bytes)
{
    // Off while one is recorded: interning a filename allocates from the
    // kernel heap, which cannot re-enter obj_alloc, but a future caller could.
    tm_rec = false;
    u32 tb = capture();
    if (tb != NO_ID) {
        Ent e{ Value::of_obj(o).w, tb, u32(bytes) };
        if (tm->traces.push(e)) {
            if (tm->at.insert(e.key, u32(tm->traces.size() - 1))) {
                tm->traced += bytes;
                if (tm->traced > tm->peak)
                    tm->peak = tm->traced;
            } else {
                tm->traces.pop();
            }
        }
    }
    tm_rec = tm_on;
}

void tm_untrack(Obj *o)
{
    u32 key = Value::of_obj(o).w;
    u32 *p  = tm->at.find(key);
    if (!p)
        return;
    usize i = *p;
    tm->traced -= tm->traces[i].size;
    tm->at.remove(key);
    // Swap the last entry down, so the walk `_get_traces` makes has no holes.
    usize last = tm->traces.size() - 1;
    if (i != last) {
        tm->traces[i] = tm->traces[last];
        // find rather than insert: insert may grow, and growing may fail.
        if (u32 *q = tm->at.find(tm->traces[i].key))
            *q = u32(i);
    }
    tm->traces.pop();
}

bool tm_start(u32 nframe)
{
    if (tm_on)
        return true; // as CPython's: the hooks are installed, and that is all
    if (!tm && !(tm = heap_new<Tm>()))
        return false;
    tm->nframe = nframe;
    tm_clear();
    tm_on = tm_rec = true;
    return true;
}

bool tracemalloc_install(DictObj *into)
{
    return mod_defs(into, DEFS);
}
