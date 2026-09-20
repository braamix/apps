// The builtins. One that must call back into Python -- `sorted(key=)`,
// `min(key=)` -- parks in a ContObj and lets the VM drive it, which is ground
// rule 2; see call.h. `map` and `filter` still wait, because they call back
// from inside the iterator protocol and py_next has no way to suspend.
#include "builtin.h"

#include "astmod.h"
#include "bigint.h"
#include "call.h"
#include "compare.h"
#include "compile.h"
#include "complex.h"
#include "exc.h"
#include "format.h"
#include "frame.h"
#include "gc.h"
#include "gen.h"
#include "genalias.h"
#include "import.h"
#include "intern.h"
#include "io.h"
#include "iter.h"
#include "kernel/alloc.h"
#include "kernel/fmt.h"
#include "kernel/text.h"
#include "lazy.h"
#include "math/ftoa.h"
#include "math/math.h"
#include "method.h"
#include "module.h"
#include "ops.h"
#include "parse.h"
#include "posix.h"
#include "type.h"
#include "ucd.h"
#include "ustr.h"
#include "vm.h"
#include "weak.h"

namespace {

// All of these outlive the process, so they are roots rather than heap that
// the collector may take: a static Value is not traced, so they live in one
// dict that is.
struct Home {
    Value builtins;
    Value builtins_mod;
    String *sink;
};

Home *home;

Home *here()
{
    if (!home)
        home = heap_new<Home>();
    return home;
}

void home_mark()
{
    if (!home)
        return;
    gc_mark(home->builtins);
    gc_mark(home->builtins_mod);
}

R oom()
{
    return err_set("MemoryError", "out of memory");
}

DictObj *dict_at(Value v)
{
    return static_cast<DictObj *>(v.obj());
}

// A builtin cannot step a generator; only the dispatch loop can. So a builtin
// that is handed one parks instead. See iter_park in call.h.
inline bool parks(const CallArgs &a, u32 at)
{
    return a.nargs > at && iter_needs_vm(a.args[at]);
}

// ------------------------------------------------------------------- print

// str() and repr() of a class instance are Python, so a builtin that shows one
// has to ask the VM for it. `text_of` says which method, if any, is wanted.
Value text_of(Value v, bool want_str)
{
    // A weak proxy's str is its referent's; its repr is its own.
    if (want_str && is_weakproxy(v)) {
        v = proxy_target(v);
        if (v.is_nil())
            return err_clear(), Value();
    }
    // A class whose metaclass writes __repr__ or __str__ is shown by it.
    if (is_meta_inst(v)) {
        Value meta = obj_value(v.obj()->type->owner);
        Value fn   = type_hook(meta, want_str ? "__str__" : "__repr__");
        if ((fn.is_nil() || is_native(fn)) && want_str)
            fn = type_hook(meta, "__repr__");
        return fn.is_nil() || is_native(fn) ? Value() : method_new(fn, v);
    }
    if (!is_inst(v))
        return Value();
    // object's own __repr__ is the native one the instance already answers.
    Root found;
    auto own = [&](Str name) {
        StrObj *n = str_intern(name);
        return n && type_lookup(inst_of(v)->cls, n, found.v) == R::Ok &&
               !is_object_default(found.v);
    };
    // str() falls back to __repr__ only where __str__ is object's, which
    // answers with the repr. str and bytes write one of their own, so a
    // subclass of either is shown as its text however it spells __repr__.
    if (want_str) {
        Value nat = inst_of(v)->native;
        if (!own("__str__") && (is_str(nat) || is_bytes(nat) || !own("__repr__")))
            return Value();
    } else if (!own("__repr__")) {
        return Value();
    }
    if (is_inst(found.v)) {
        Value out;
        return special_bind(found.v, v, inst_of(v)->cls, out) == R::Ok ? out : Value();
    }
    return method_new(found.v, v);
}

R print_line(const Value *args, u32 n, Str sep, Str end, Value file, Value &out);

// Text already rendered, standing in for the instance that answered it. A
// container's repr is C++ and cannot call Python, so an instance inside one is
// replaced by this in a *copy* before that repr ever runs.
struct RawObj : Obj {
    Value text;
};

void raw_trace(Obj *o)
{
    gc_mark(static_cast<RawObj *>(o)->text);
}

R raw_repr(Value v, String &out)
{
    Value t = static_cast<RawObj *>(v.obj())->text;
    return out.append(str_of(t)->str()) ? R::Ok : oom();
}

constexpr Type raw_type{ .name = "str", .trace = raw_trace, .repr = raw_repr, .str = raw_repr };

Value raw_new(Value text)
{
    Root rt{ text };
    RawObj *o = static_cast<RawObj *>(obj_alloc(&raw_type, sizeof(RawObj)));
    if (!o)
        return oom(), Value();
    o->text = rt.v;
    return obj_value(o);
}

constexpr u32 SHOW_DEEP = 8; // a container holding itself stops here

// Every instance inside `v` that answers __repr__, in the order a repr reaches
// them, as the bound method to call. Containers only; `v` itself is not one.
bool collect_nested(Value v, ListObj *into, u32 depth);

bool collect_one(Value v, ListObj *into, u32 depth)
{
    Value m = text_of(v, false);
    if (!m.is_nil())
        return list_push(into, m);
    return collect_nested(v, into, depth);
}

bool collect_nested(Value v, ListObj *into, u32 depth)
{
    if (depth >= SHOW_DEEP)
        return true;
    if (is_tuple(v)) {
        TupleObj *t = static_cast<TupleObj *>(v.obj());
        for (usize i = 0; i < t->len; i++)
            if (!collect_one(t->items()[i], into, depth + 1))
                return false;
    } else if (is_list(v)) {
        ListObj *l = list_of(v);
        for (usize i = 0; i < l->items.size(); i++)
            if (!collect_one(l->items[i], into, depth + 1))
                return false;
    } else if (is_anydict(v) || is_set(v)) {
        const Table &t =
            is_anydict(v) ? static_cast<DictObj *>(v.obj())->t : static_cast<SetObj *>(v.obj())->t;
        usize at = 0;
        Value k, x;
        while (table_next(t, at, k, x)) {
            if (!collect_one(k, into, depth + 1))
                return false;
            if (is_anydict(v) && !collect_one(x, into, depth + 1))
                return false;
        }
    }
    return true;
}

Value rewrite(Value v, ListObj *text, usize &at, u32 depth);

// The same walk, taking the rendered text in the same order. A container that
// holds one is copied; one that does not is left alone.
Value rewrite_one(Value v, ListObj *text, usize &at, u32 depth)
{
    if (!text_of(v, false).is_nil()) {
        if (at >= text->items.size())
            return v;
        return raw_new(text->items[at++]);
    }
    return rewrite(v, text, at, depth);
}

Value rewrite(Value v, ListObj *text, usize &at, u32 depth)
{
    if (depth >= SHOW_DEEP)
        return v;
    Root rv{ v };
    if (is_tuple(rv.v)) {
        usize n     = static_cast<TupleObj *>(rv.v.obj())->len;
        TupleObj *t = tuple_new(n);
        if (!t)
            return oom(), Value();
        Root rt{ obj_value(t) };
        for (usize i = 0; i < n; i++) {
            Value x =
                rewrite_one(static_cast<TupleObj *>(rv.v.obj())->items()[i], text, at, depth + 1);
            if (x.is_nil())
                return Value();
            static_cast<TupleObj *>(rt.v.obj())->items()[i] = x;
        }
        return rt.v;
    }
    if (is_list(rv.v)) {
        ListObj *l = list_new();
        if (!l)
            return oom(), Value();
        Root rl{ obj_value(l) };
        for (usize i = 0; i < list_of(rv.v)->items.size(); i++) {
            Value x = rewrite_one(list_of(rv.v)->items[i], text, at, depth + 1);
            if (x.is_nil() || !list_push(list_of(rl.v), x))
                return x.is_nil() ? Value() : (oom(), Value());
        }
        return rl.v;
    }
    if (is_anydict(rv.v) || is_set(rv.v)) {
        bool dict = is_anydict(rv.v);
        Root out{ dict ? obj_value(dict_new()) : obj_value(set_new()) };
        if (out.v.is_nil())
            return oom(), Value();
        usize step = 0;
        Value k, x;
        for (;;) {
            const Table &t =
                dict ? static_cast<DictObj *>(rv.v.obj())->t : static_cast<SetObj *>(rv.v.obj())->t;
            if (!table_next(t, step, k, x))
                break;
            Root nk{ rewrite_one(k, text, at, depth + 1) };
            if (nk.v.is_nil())
                return Value();
            if (!dict) {
                if (set_add(static_cast<SetObj *>(out.v.obj()), nk.v) != R::Ok)
                    return Value();
                continue;
            }
            Value nx = rewrite_one(x, text, at, depth + 1);
            if (nx.is_nil() || dict_set(static_cast<DictObj *>(out.v.obj()), nk.v, nx) != R::Ok)
                return Value();
        }
        if (is_frozendict(rv.v))
            out.v.obj()->type = &frozendict_type;
        return out.v;
    }
    return rv.v;
}

// s[0] the values, s[1] the method waiting on an answer, s[2] sep, s[3] end.
// Every instance becomes the string its __str__ answers, in place; then the
// ordinary printing runs over a list that needs no more Python.
R print_step(ContObj *k, Value in)
{
    ListObj *xs = list_of(k->s[0]);
    if (!k->s[1].is_nil()) {
        if (!is_str(in))
            return err_set2("TypeError", "__str__ returned a non-string", type_name(in));
        xs->items[k->i - 1] = in;
        k->s[1]             = Value();
    } else if (!k->s[5].is_nil() && k->j > list_of(k->s[5])->items.size()) {
        if (!is_str(in))
            return err_set2("TypeError", "__repr__ returned a non-string", type_name(in));
        if (!list_push(list_of(k->s[5]), in))
            return oom();
    }
    while (k->i < xs->items.size()) {
        Value m = text_of(xs->items[k->i], true);
        k->i++;
        if (!m.is_nil()) {
            k->s[1] = m;
            return cont_call(k, m, Value(), 0);
        }
    }

    // The arguments are showable; anything nested inside one is not. Gather
    // those, render them one call at a time, and put the text back in a copy.
    if (k->s[4].is_nil()) {
        // The first is parked in the continuation before the second is made:
        // making one allocates, and a fresh list with nothing pointing at it
        // is exactly what a collection there would take.
        ListObj *need = list_new();
        if (!need)
            return oom();
        k->s[4]       = obj_value(need);
        ListObj *done = list_new();
        if (!done)
            return oom();
        k->s[5] = obj_value(done);
        for (usize i = 0; i < xs->items.size(); i++)
            if (!collect_nested(xs->items[i], list_of(k->s[4]), 0))
                return oom();
    }
    ListObj *need = list_of(k->s[4]);
    if (k->j < need->items.size())
        return cont_call(k, need->items[k->j++], Value(), 0);
    if (need->items.size()) {
        usize at = 0;
        for (usize i = 0; i < xs->items.size(); i++) {
            Value x = rewrite(xs->items[i], list_of(k->s[5]), at, 0);
            if (x.is_nil())
                return R::Err;
            xs->items[i] = x;
        }
    }

    Str sep = is_str(k->s[2]) ? str_of(k->s[2])->str() : Str(" ");
    Str end = is_str(k->s[3]) ? str_of(k->s[3])->str() : Str("\n");
    Value wrote;
    if (print_line(xs->items.data(), u32(xs->items.size()), sep, end, k->s[6], wrote) != R::Ok)
        return R::Err;
    return cont_done(k, wrote.is_nil() ? value_none() : wrote);
}

R flush_after_step(ContObj *k, Value);

// The print's continuation, or Nil, and then file.flush().
R flush_after(Value first, Value file, Value &out)
{
    Root rf{ first }, rfile{ file };
    Root kv{ cont_new(flush_after_step) };
    if (kv.v.is_nil())
        return R::Err;
    cont_of(kv.v)->s[0] = is_cont(rf.v) ? rf.v : Value();
    cont_of(kv.v)->s[1] = rfile.v;
    out                 = kv.v;
    return R::Ok;
}

R b_print(const CallArgs &a, Value &out)
{
    Str sep = " ", end = "\n";
    bool flush = false;
    Root file;
    for (u32 k = 0; k < a.nkw; k++) {
        Str name = is_str(a.kwnames[k]) ? str_of(a.kwnames[k])->str() : Str();
        if (name == "file") {
            file = a.kwvals[k];
            continue;
        }
        if (name == "flush") {
            flush = py_truth(a.kwvals[k]);
            continue;
        }
        if (is_none(a.kwvals[k]) && (name == "sep" || name == "end"))
            continue;
        if (!is_str(a.kwvals[k])) {
            if (name != "sep" && name != "end")
                return err_set2("TypeError", "print() got an unexpected keyword argument", name);
            Buf<96> m;
            m.put(name).put(" must be None or a string, not ").put(type_name(a.kwvals[k]));
            return err_set("TypeError", m.str());
        }
        if (name == "sep")
            sep = str_of(a.kwvals[k])->str();
        else if (name == "end")
            end = str_of(a.kwvals[k])->str();
        else
            return err_set2("TypeError", "print() got an unexpected keyword argument", name);
    }

    bool any = false;
    for (u32 i = 0; i < a.nargs && !any; i++) {
        if (!text_of(a.args[i], true).is_nil()) {
            any = true;
            break;
        }
        ListObj *probe = list_new();
        if (!probe)
            return oom();
        Root rp{ obj_value(probe) };
        if (!collect_nested(a.args[i], list_of(rp.v), 0))
            return oom();
        any = list_of(rp.v)->items.size() != 0;
    }
    if (any) {
        ListObj *l = list_new();
        if (!l)
            return oom();
        Root rl{ obj_value(l) };
        for (u32 i = 0; i < a.nargs; i++)
            if (!list_push(list_of(rl.v), a.args[i]))
                return oom();
        Root sv{ str_new(sep) }, ev{ str_new(end) };
        if (sv.v.is_nil() || ev.v.is_nil())
            return R::Err;
        Root kv{ cont_new(print_step) };
        if (kv.v.is_nil())
            return R::Err;
        cont_of(kv.v)->s[0] = rl.v;
        cont_of(kv.v)->s[2] = sv.v;
        cont_of(kv.v)->s[3] = ev.v;
        cont_of(kv.v)->s[6] = file.v;
        out                 = kv.v;
        return flush ? flush_after(kv.v, file.v, out) : R::Ok;
    }
    Value wrote;
    if (print_line(a.args, a.nargs, sep, end, file.v, wrote) != R::Ok)
        return R::Err;
    out = wrote.is_nil() ? value_none() : wrote;
    if (flush)
        return flush_after(wrote, file.v, out);
    return R::Ok;
}

// The whole line, then wherever sys.stdout says it goes. `out` comes back a
// ContObj when the program has put an object of its own there, and Nil when
// the text has already been buffered.
R print_line(const Value *args, u32 n, Str sep, Str end, Value file, Value &out)
{
    Roots pin{ const_cast<Value *>(args), n };
    Root rf{ file };
    // Where each piece starts, for a file of the program's own: it is
    // written a piece at a time, separators and end included.
    String line;
    Vec<usize> cuts;
    if (!cuts.push(0))
        return oom();
    for (u32 i = 0; i < n; i++) {
        if (i && (!line.append(sep) || !cuts.push(line.size())))
            return oom();
        if (py_str(args[i], line) != R::Ok)
            return R::Err;
        if (!cuts.push(line.size()))
            return oom();
    }
    if (!line.append(end) || !cuts.push(line.size()))
        return oom();
    return sys_write(rf.v, line.str(), out, Span<const usize>(cuts.data(), cuts.size()));
}

// ------------------------------------------------------------- conversions

// A builtin whose work on a class instance is one special method. `j` says
// what the answer has to be.
// WANT_FOUND is hasattr's: True because the call returned at all, whatever
// it returned. WANT_BOOL is __bool__'s, which is the value's own truth.
enum : u32 { WANT_ANY, WANT_INT, WANT_STR, WANT_BYTES, WANT_BOOL, WANT_FOUND, WANT_HASH };

R one_step(ContObj *k, Value in)
{
    if (k->i++ == 0)
        return cont_call(k, k->s[0], k->a[0], k->nargs);
    // An AttributeError this caught means getattr's default, or hasattr False.
    if (in.is_nil())
        return cont_done(k, k->j == WANT_BOOL || k->j == WANT_FOUND ? value_bool(false) : k->s[1]);
    i64 n = 0;
    switch (k->j) {
    case WANT_INT:
        if (!is_intval(in))
            return err_set2("TypeError", "a special method returned a non-integer", type_name(in));
        break;
    case WANT_HASH:
        // A __hash__ past the width of a hash is hashed as an int, as
        // CPython does; -1 is said as -2.
        if (!is_intval(in))
            return err_set2("TypeError", "__hash__ returned a non-integer", type_name(in));
        if (!as_index(in, n) || n < -0x7fffffffll - 1 || n > 0x7fffffffll)
            n = i32(int_hash_of(in));
        in = int_from_i64(n == -1 ? -2 : n);
        if (in.is_nil())
            return R::Err;
        break;
    case WANT_STR:
        if (!is_str(in))
            return err_set2("TypeError", "a special method returned a non-string", type_name(in));
        break;
    case WANT_BYTES:
        if (!is_bytes(in))
            return err_set2("TypeError", "__bytes__ returned a non-bytes", type_name(in));
        break;
    case WANT_BOOL:
        in = value_bool(py_truth(in));
        break;
    case WANT_FOUND:
        in = value_bool(true);
        break;
    default:
        break;
    }
    return cont_done(k, in);
}

// Nil and no error when the class has no such method.
Value one_special(Value v, Str name, u32 want)
{
    Root m{ type_special(v, name) };
    if (m.v.is_nil())
        return Value();
    Value kv = cont_new(one_step);
    if (kv.is_nil())
        return Value();
    cont_of(kv)->s[0] = m.v;
    cont_of(kv)->j    = want;
    return kv;
}

R b_len(const CallArgs &a, Value &out)
{
    if (!args_only(a, "len", 1, 1))
        return R::Err;
    out = one_special(a.args[0], "__len__", WANT_INT);
    if (!out.is_nil())
        return R::Ok;
    if (err_pending())
        return R::Err;
    usize n = 0;
    if (py_len(a.args[0], n) != R::Ok)
        return R::Err;
    out = int_from_i64(i64(n));
    return out.is_nil() ? R::Err : R::Ok;
}

R b_abs(const CallArgs &a, Value &out)
{
    if (!args_only(a, "abs", 1, 1))
        return R::Err;
    out = one_special(a.args[0], "__abs__", WANT_ANY);
    if (!out.is_nil())
        return R::Ok;
    if (err_pending())
        return R::Err;
    if (is_intval(a.args[0]))
        return int_absolute(a.args[0], out);
    if (is_complex(a.args[0]))
        return complex_abs(a.args[0], out);
    if (is_float(a.args[0])) {
        f64 v = float_of(a.args[0]);
        out   = float_new(v < 0 ? -v : v);
        return out.is_nil() ? R::Err : R::Ok;
    }
    return err_set2("TypeError", "bad operand type for abs()", type_name(a.args[0]));
}

// Which text is wanted, and what an instance nested in a container answers
// with. `nargs` on the continuation carries it.
enum : u32 { SHOW_REPR, SHOW_STR, SHOW_ASCII };

// The text is in hand; s[4] is a format spec still to apply to it, or Nil.
R show_done(ContObj *k, Value text)
{
    if (k->s[4].is_nil())
        return cont_done(k, text);
    String out;
    if (format_builtin(text, str_of(k->s[4])->str(), out) != R::Ok)
        return R::Err;
    Value made = str_new(out.str());
    return made.is_nil() ? R::Err : cont_done(k, made);
}

// s[0] is the value, s[1] its own __str__ or __repr__ if it has one, s[2] the
// instances nested inside it and s[3] the text they answered; j says str.
R show_step(ContObj *k, Value in)
{
    if (k->i++ == 0) {
        if (!k->s[1].is_nil())
            return cont_call(k, k->s[1], Value(), 0);
    } else if (!k->s[1].is_nil() && k->i == 2) {
        if (!is_str(in))
            return err_set2("TypeError", "__repr__ returned a non-string", type_name(in));
        if (k->nargs != SHOW_ASCII)
            return show_done(k, in);
        String esc;
        if (py_ascii(in, esc) != R::Ok)
            return R::Err;
        Value made = str_new(esc.str());
        return made.is_nil() ? R::Err : show_done(k, made);
    } else if (k->j > 0) {
        if (!is_str(in))
            return err_set2("TypeError", "__repr__ returned a non-string", type_name(in));
        if (!list_push(list_of(k->s[3]), in))
            return oom();
    }

    ListObj *need = list_of(k->s[2]);
    if (k->j < need->items.size())
        return cont_call(k, need->items[k->j++], Value(), 0);

    usize at = 0;
    Root done{ rewrite(k->s[0], list_of(k->s[3]), at, 0) };
    if (done.v.is_nil())
        return R::Err;
    String text;
    R r = k->nargs == SHOW_STR     ? py_str(done.v, text)
          : k->nargs == SHOW_ASCII ? py_ascii(done.v, text)
                                   : py_repr(done.v, text);
    if (r != R::Ok)
        return R::Err;
    Value made = str_new(text.str());
    return made.is_nil() ? R::Err : show_done(k, made);
}

R show(Value v, u32 want, Value &out)
{
    Root rv{ v }, m{ text_of(v, want == SHOW_STR) };
    Root need{ obj_value(list_new()) }, done{ obj_value(list_new()) };
    if (need.v.is_nil() || done.v.is_nil())
        return oom();
    if (m.v.is_nil() && !collect_nested(rv.v, list_of(need.v), 0))
        return oom();
    if (!m.v.is_nil() || list_of(need.v)->items.size()) {
        Value kv = cont_new(show_step);
        if (kv.is_nil())
            return R::Err;
        cont_of(kv)->s[0]  = rv.v;
        cont_of(kv)->s[1]  = m.v;
        cont_of(kv)->s[2]  = need.v;
        cont_of(kv)->s[3]  = done.v;
        cont_of(kv)->nargs = want;
        out                = kv;
        return R::Ok;
    }
    String text;
    R r = want == SHOW_STR     ? py_str(rv.v, text)
          : want == SHOW_ASCII ? py_ascii(rv.v, text)
                               : py_repr(rv.v, text);
    if (r != R::Ok)
        return R::Err;
    out = str_new(text.str());
    return out.is_nil() ? R::Err : R::Ok;
}

R b_repr(const CallArgs &a, Value &out)
{
    if (!args_only(a, "repr", 1, 1))
        return R::Err;
    return show(a.args[0], SHOW_REPR, out);
}

// hex(), oct() and bin(): the prefix and the magnitude, sign in front.
R radix_show(const CallArgs &a, Str who, u32 base, Str prefix, Value &out)
{
    if (!args_only(a, who, 1, 1))
        return R::Err;
    Value v = a.args[0];
    if (is_inst(v) && is_intval(inst_of(v)->native))
        v = inst_of(v)->native;
    if (!is_intval(v))
        return err_set2("TypeError", "an integer is required", type_name(v));
    String text;
    if (int_is_neg(v) && !text.push('-'))
        return oom();
    if (!text.append(prefix))
        return oom();
    if (int_digits(v, base, false, text) != R::Ok)
        return R::Err;
    out = str_new(text.str());
    return out.is_nil() ? R::Err : R::Ok;
}

R b_hex(const CallArgs &a, Value &out)
{
    R r = R::Ok;
    if (redo_converted(a, 0, "__index__", b_hex, out, r))
        return r;
    return radix_show(a, "hex", 16, "0x", out);
}

R b_oct(const CallArgs &a, Value &out)
{
    R r = R::Ok;
    if (redo_converted(a, 0, "__index__", b_oct, out, r))
        return r;
    return radix_show(a, "oct", 8, "0o", out);
}

R b_bin(const CallArgs &a, Value &out)
{
    R r = R::Ok;
    if (redo_converted(a, 0, "__index__", b_bin, out, r))
        return r;
    return radix_show(a, "bin", 2, "0b", out);
}

R b_ascii(const CallArgs &a, Value &out)
{
    if (!args_only(a, "ascii", 1, 1))
        return R::Err;
    return show(a.args[0], SHOW_ASCII, out);
}

// __format__ answered in Python, as one call. A class that writes none still
// reaches object.__format__, which is format_builtin's last arm.
R format_step(ContObj *k, Value in)
{
    if (k->i++ == 0)
        return cont_call(k, k->s[0], k->a[0], 1);
    if (!is_str(in))
        return err_set2("TypeError", "__format__ must return a str", type_name(in));
    return cont_done(k, in);
}

R b_format(const CallArgs &a, Value &out)
{
    if (!args_only(a, "format", 1, 2))
        return R::Err;
    if (a.nargs > 1 && !is_str(a.args[1]))
        return err_set2("TypeError", "format() argument 2 must be str", type_name(a.args[1]));
    Str spec = a.nargs > 1 ? str_of(a.args[1])->str() : Str("");

    out = format_special(a.args[0], spec);
    if (!out.is_nil())
        return R::Ok;
    if (err_pending())
        return R::Err;
    // No __format__ of its own: object.__format__ is str(self), and that may
    // be Python too.
    if (is_inst(a.args[0]) && !spec.size()) {
        out = show_special(a.args[0], true);
        if (!out.is_nil())
            return R::Ok;
        if (err_pending())
            return R::Err;
    }
    String text;
    if (format_builtin(a.args[0], spec, text) != R::Ok)
        return R::Err;
    out = str_new(text.str());
    return out.is_nil() ? R::Err : R::Ok;
}

// str(), bytes() and bytearray() take (object, encoding, errors), by
// position or by name. `n` counts what was given.
bool text_args(const CallArgs &a, Str who, Value got[3], u32 &n)
{
    static const Str NAMES[] = { "", "encoding", "errors" };
    got[0] = got[1] = got[2] = Value();
    if (a.nargs > 3) {
        Buf<96> b;
        b.put(who).put("() takes at most 3 arguments");
        return err_set("TypeError", b.str()), false;
    }
    for (u32 i = 0; i < a.nargs; i++)
        got[i] = a.args[i];
    for (u32 k = 0; k < a.nkw; k++) {
        Str nm = is_str(a.kwnames[k]) ? str_of(a.kwnames[k])->str() : Str();
        u32 i  = nm == (who == "str" ? Str("object") : Str("source")) ? 0 : 1;
        while (i < 3 && !(NAMES[i] == nm) && i)
            i++;
        if (i == 3 || !got[i].is_nil()) {
            Buf<128> b;
            b.put(who).put("() got an unexpected keyword argument '").put(nm).put("'");
            return err_set("TypeError", b.str()), false;
        }
        got[i] = a.kwvals[k];
    }
    n = 0;
    for (u32 i = 0; i < 3; i++)
        if (!got[i].is_nil())
            n = i + 1;
    for (u32 i = 1; i < 3; i++)
        if (!got[i].is_nil() && !is_str(got[i])) {
            Buf<96> b;
            b.put(who).put("() argument '").put(NAMES[i]).put("' must be str");
            return err_not(b.str(), got[i]), false;
        }
    return true;
}

R b_str(const CallArgs &a, Value &out)
{
    Value got[3];
    u32 n = 0;
    if (!text_args(a, "str", got, n))
        return R::Err;
    if (got[0].is_nil() && n == 0) {
        out = str_new("");
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (!got[1].is_nil() || !got[2].is_nil()) {
        if (got[0].is_nil())
            got[0] = bytes_new(Str());
        return text_decode(got[0], got[1], got[2], out);
    }
    if (is_str(got[0])) {
        out = got[0];
        return R::Ok;
    }
    return show(got[0], SHOW_STR, out);
}

R b_bool(const CallArgs &a, Value &out)
{
    if (!args_only(a, "bool", 0, 1))
        return R::Err;
    if (a.nargs) {
        out = one_special(a.args[0], "__bool__", WANT_BOOL);
        if (!out.is_nil())
            return R::Ok;
        if (err_pending())
            return R::Err;
        out = one_special(a.args[0], "__len__", WANT_BOOL);
        if (!out.is_nil())
            return R::Ok;
        if (err_pending())
            return R::Err;
    }
    out = value_bool(a.nargs && py_truth(a.args[0]));
    return err_pending() ? R::Err : R::Ok;
}

// A str's digits and spaces in ASCII, which is all the number grammars read.
// `text` is left alone for bytes and for ASCII.
bool number_text(Value obj, Str &text, String &buf)
{
    if (!is_str(obj) || (obj.obj()->flags & OBJ_ASCII))
        return true;
    if (!ucd_ascii_number(text, buf))
        return oom() == R::Ok;
    text = buf.str();
    return true;
}

// "invalid literal for int() with base 10: 'x'": the repr, cut at 200.
R bad_number(Str head, Value obj)
{
    Root ro{ obj };
    String r;
    if (py_repr(ro.v, r) != R::Ok)
        return R::Err;
    Str shown = r.str();
    usize at = 0, n = 0;
    while (at < shown.size() && n < 200) {
        at += cp_width(u8(shown[at]));
        n++;
    }
    return err_set2("ValueError", head, shown.substr(0, at));
}

// int(s, base): space, a sign, an optional 0x/0o/0b prefix, then digits with
// `_` allowed between them, over any width. bigint.cpp owns the grammar.
R int_of_text(Value obj, Str s, i64 base, Value &out)
{
    if (base != 0 && (base < 2 || base > 36))
        return err_set("ValueError", "int() base must be >= 2 and <= 36, or 0");
    Root ro{ obj };
    String buf;
    if (!number_text(ro.v, s, buf))
        return R::Err;
    out = int_parse(s, u32(base), true);
    if (!out.is_nil())
        return R::Ok;
    if (err_kind() != "ValueError" || err_message() != "invalid literal for int()")
        return R::Err;
    err_clear();
    Buf<64> m;
    m.put("invalid literal for int() with base ").put(u32(base ? base : 10));
    return bad_number(m.str(), ro.v);
}

R b_int(const CallArgs &a, Value &out)
{
    // int(x, base=b): the base alone may be a keyword.
    if (a.nkw == 1 && is_str(a.kwnames[0]) && str_of(a.kwnames[0])->str() == "base" &&
        a.nargs == 1) {
        Value two[2] = { a.args[0], a.kwvals[0] };
        CallArgs b;
        b.args  = two;
        b.nargs = 2;
        return b_int(b, out);
    }
    if (!args_only(a, "int", 0, 2))
        return R::Err;
    if (!a.nargs) {
        out = Value::of_int(0);
        return R::Ok;
    }
    Str text;
    bool textual =
        is_str(a.args[0]) ? (text = str_of(a.args[0])->str(), true) : bytes_like(a.args[0], text);
    if (a.nargs > 1) {
        i64 base = 0;
        if (!as_index(a.args[1], base))
            return err_set2("TypeError", "int() base must be an integer", type_name(a.args[1]));
        if (!textual)
            return err_set("TypeError", "int() can't convert non-string with an explicit base");
        return int_of_text(a.args[0], text, base, out);
    }
    out = one_special(a.args[0], "__int__", WANT_INT);
    if (!out.is_nil())
        return R::Ok;
    if (err_pending())
        return R::Err;
    out = one_special(a.args[0], "__index__", WANT_INT);
    if (!out.is_nil())
        return R::Ok;
    if (err_pending())
        return R::Err;
    // A subclass of int, float or str stands for the value inside it.
    Value v = is_inst(a.args[0]) ? method_self(a.args[0]) : a.args[0];
    if (is_intval(v)) {
        // int(True) is 1, so bool does not simply pass through.
        out = is_bool(v) ? Value::of_int(is_true(v) ? 1 : 0) : v;
        return R::Ok;
    }
    if (!textual && v != a.args[0])
        textual = is_str(v) ? (text = str_of(v)->str(), true) : bytes_like(v, text);
    if (is_float(v)) {
        f64 x = float_of(v);
        if (isnan(x))
            return err_set("ValueError", "cannot convert float NaN to integer");
        if (isinf(x))
            return err_set("OverflowError", "cannot convert float infinity to integer");
        // Toward zero, which is what Python asks for.
        out = int_from_f64(x);
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (textual)
        return int_of_text(a.args[0], text, 10, out);
    return err_set2("TypeError", "int() argument must be a number or a string",
                    type_name(a.args[0]));
}

R b_ord(const CallArgs &a, Value &out)
{
    if (!args_only(a, "ord", 1, 1))
        return R::Err;
    if (is_bytes(a.args[0])) {
        BytesObj *b = static_cast<BytesObj *>(a.args[0].obj());
        if (b->len != 1)
            return err_set("TypeError", "ord() expected a character");
        out = Value::of_int(b->data()[0]);
        return R::Ok;
    }
    if (!is_str(a.args[0]) || str_of(a.args[0])->chars != 1)
        return err_set2("TypeError", "ord() expected a character", type_name(a.args[0]));
    out = Value::of_int(i32(str_char_at(str_of(a.args[0]), 0)));
    return R::Ok;
}

R b_chr(const CallArgs &a, Value &out)
{
    if (!args_only(a, "chr", 1, 1))
        return R::Err;
    i64 cp = 0;
    if (!as_index(a.args[0], cp))
        return err_set2("TypeError", "chr() argument must be an integer", type_name(a.args[0]));
    if (cp < 0 || cp > 0x10ffff)
        return err_set("ValueError", "chr() arg not in range(0x110000)");

    // A surrogate is a character too, until something encodes it.
    char b[4];
    usize n = cp_encode(u32(cp), b);
    out     = obj_value(str_raw(Str(b, n)));
    return out.is_nil() ? oom() : R::Ok;
}

// bytes(s, encoding[, errors]) and the same for bytearray: the octets, or a
// ContObj when a codec written in Python has to make them.
R encoded(const CallArgs &a, Str who, Value &out)
{
    Value got[3];
    u32 n = 0;
    if (!text_args(a, who, got, n))
        return R::Err;
    if (got[1].is_nil()) {
        if (is_str(got[0]))
            return err_set("TypeError", "string argument without an encoding");
        return err_set("TypeError", "errors without a string argument");
    }
    if (!is_str(got[0]))
        return err_set("TypeError", "encoding without a string argument");
    return text_encode(got[0], got[1], got[2], out);
}

R b_bytes(const CallArgs &a, Value &out)
{
    if (a.nkw || a.nargs > 1)
        return encoded(a, "bytes", out);
    if (!a.nargs || is_bytes(a.args[0])) {
        out = a.nargs ? a.args[0] : bytes_new(Str());
        return out.is_nil() ? R::Err : R::Ok;
    }
    // A class writing __bytes__ answers with it, before the buffer, the
    // sequence of byte values and the iteration that would drain one: a
    // message is indexable and would otherwise be read as its own headers.
    out = one_special(a.args[0], "__bytes__", WANT_BYTES);
    if (!out.is_nil())
        return R::Ok;
    if (err_pending())
        return R::Err;
    if (a.nargs == 1 && parks(a, 0))
        return iter_park(a, 0, b_bytes, out);
    Str octets;
    if (bytes_like(a.args[0], octets)) {
        out = bytes_new(octets);
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (is_str(a.args[0]))
        return err_set("TypeError", "string argument without an encoding");
    i64 n = 0;
    if (as_index(a.args[0], n)) {
        if (n < 0)
            return err_set("ValueError", "negative count");
        String zeros;
        for (i64 i = 0; i < n; i++)
            if (!zeros.push('\0'))
                return oom();
        out = bytes_new(zeros.str());
        return out.is_nil() ? R::Err : R::Ok;
    }
    // Anything else is a sequence of byte values.
    Root it{ py_iter(a.args[0]) };
    if (it.v.is_nil())
        return R::Err;
    String bs;
    for (;;) {
        Root got;
        R r = py_next(it.v, got.v);
        if (r == R::Err)
            return R::Err;
        if (r == R::NotImpl)
            break;
        i64 b = 0;
        if (!as_index(got.v, b) || b < 0 || b > 255)
            return err_set("ValueError", "bytes must be in range(0, 256)");
        if (!bs.push(char(b)))
            return oom();
    }
    out = bytes_new(bs.str());
    return out.is_nil() ? R::Err : R::Ok;
}

// complex(), complex(z), complex(re, im) and complex("1+2j").
R b_complex(const CallArgs &a, Value &out)
{
    if (!args_only(a, "complex", 0, 2))
        return R::Err;
    R r = R::Ok;
    if (redo_converted(a, 0, "__complex__", b_complex, out, r) ||
        redo_converted(a, 0, "__float__", b_complex, out, r) ||
        redo_converted(a, 0, "__index__", b_complex, out, r) ||
        redo_converted(a, 1, "__float__", b_complex, out, r) ||
        redo_converted(a, 1, "__index__", b_complex, out, r))
        return r;
    if (!a.nargs) {
        out = complex_new(0, 0);
        return out.is_nil() ? R::Err : R::Ok;
    }
    // An instance of a complex subclass is that complex.
    for (u32 i = 0; i < a.nargs; i++)
        if (is_inst(a.args[i]) && is_complex(inst_of(a.args[i])->native)) {
            Value args[2] = { a.args[0], a.nargs > 1 ? a.args[1] : Value() };
            for (u32 k = 0; k < a.nargs; k++)
                if (is_inst(args[k]) && is_complex(inst_of(args[k])->native))
                    args[k] = inst_of(args[k])->native;
            CallArgs b = a;
            b.args     = args;
            return b_complex(b, out);
        }
    if (is_str(a.args[0])) {
        if (a.nargs > 1)
            return err_set("TypeError", "complex() can't take second arg if first is a string");
        f64 re = 0, im = 0;
        Str text = str_of(a.args[0])->str();
        String buf;
        if (!number_text(a.args[0], text, buf))
            return R::Err;
        if (!complex_parse(text, re, im))
            return err_set("ValueError", "complex() arg is a malformed string");
        out = complex_new(re, im);
        return out.is_nil() ? R::Err : R::Ok;
    }
    f64 re = 0, im = 0;
    if (is_complex(a.args[0])) {
        re = complex_of(a.args[0])->re;
        im = complex_of(a.args[0])->im;
    } else if (!as_number(a.args[0], re)) {
        return err_set2("TypeError", "complex() argument must be a number", type_name(a.args[0]));
    }
    if (a.nargs > 1) {
        if (is_complex(a.args[1]) || is_complex(a.args[0])) {
            f64 r2 = 0, i2 = 0;
            if (is_complex(a.args[1])) {
                r2 = complex_of(a.args[1])->re;
                i2 = complex_of(a.args[1])->im;
            } else if (!as_number(a.args[1], r2)) {
                return err_set2("TypeError", "complex() argument must be a number",
                                type_name(a.args[1]));
            }
            // complex(a, b) is a + b*1j, so an imaginary second argument
            // folds back into the real part.
            re -= i2;
            im += r2;
        } else if (!as_number(a.args[1], im)) {
            // Two plain reals go straight in: adding would turn the -0.0 of
            // `complex(1, -0.0)` into a positive zero.
            return err_set2("TypeError", "complex() argument must be a number",
                            type_name(a.args[1]));
        }
    }
    if (int_too_wide(a.args[0], re) || (a.nargs > 1 && int_too_wide(a.args[1], im)))
        return R::Err;
    out = complex_new(re, im);
    return out.is_nil() ? R::Err : R::Ok;
}

// float("1.5"), float(" nan "), float("1_000.5"): strtod's grammar with the
// space trimmed and the underscores taken out, which is what CPython's
// float() takes and parse_f64 does not.
bool float_of_text(Str s, f64 &out)
{
    String clean;
    usize at = 0, end = s.size();
    while (at < end && is_space(s[at]))
        at++;
    while (end > at && is_space(s[end - 1]))
        end--;
    for (usize i = at; i < end; i++) {
        // An underscore is only legal between digits, which is what makes
        // "1__0" and "_1" errors rather than one.
        if (s[i] == '_') {
            if (i == at || i + 1 >= end || !is_digit(s[i - 1]) || !is_digit(s[i + 1]))
                return false;
            continue;
        }
        if (!clean.push(s[i]))
            return false;
    }
    // strtod takes hexadecimal and Python's float() does not.
    Str body   = clean.str();
    usize sign = body.size() && (body[0] == '+' || body[0] == '-') ? 1 : 0;
    if (body.size() > sign + 1 && body[sign] == '0' &&
        (body[sign + 1] == 'x' || body[sign + 1] == 'X'))
        return false;
    Option<f64> got = parse_f64(body);
    if (!got.has_value())
        return false;
    out = got.value();
    return true;
}

R b_float(const CallArgs &a, Value &out)
{
    if (!args_only(a, "float", 0, 1))
        return R::Err;
    R r = R::Ok;
    if (redo_converted(a, 0, "__float__", b_float, out, r) ||
        redo_converted(a, 0, "__index__", b_float, out, r))
        return r;
    f64 v      = 0;
    Value self = a.nargs ? method_self(a.args[0]) : Value();
    if (a.nargs && self != a.args[0] && as_number(self, v)) {
        if (int_too_wide(self, v))
            return R::Err;
        out = float_new(v);
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (a.nargs && as_number(a.args[0], v) && int_too_wide(a.args[0], v))
        return R::Err;
    if (a.nargs && !as_number(a.args[0], v)) {
        Str text;
        if (is_str(a.args[0]))
            text = str_of(a.args[0])->str();
        else if (!bytes_like(a.args[0], text))
            return err_set2("TypeError", "float() argument must be a number or a string",
                            type_name(a.args[0]));
        String buf;
        if (!number_text(a.args[0], text, buf))
            return R::Err;
        if (!float_of_text(text, v))
            return bad_number("could not convert string to float", a.args[0]);
    }
    out = float_new(v);
    return out.is_nil() ? R::Err : R::Ok;
}

} // namespace

R py_float_of(Value v, Value &out)
{
    CallArgs a;
    a.args  = &v;
    a.nargs = 1;
    return b_float(a, out);
}

namespace {

// ------------------------------------------------------------- collections

R b_list(const CallArgs &a, Value &out)
{
    if (!args_only(a, "list", 0, 1))
        return R::Err;
    if (parks(a, 0))
        return iter_park(a, 0, b_list, out);
    ListObj *l = a.nargs ? py_list_of(a.args[0]) : list_new();
    if (!l)
        return err_pending() ? R::Err : oom();
    out = obj_value(l);
    return R::Ok;
}

R b_tuple(const CallArgs &a, Value &out)
{
    if (!args_only(a, "tuple", 0, 1))
        return R::Err;
    if (parks(a, 0))
        return iter_park(a, 0, b_tuple, out);
    if (!a.nargs) {
        TupleObj *e = tuple_new(0);
        if (!e)
            return oom();
        out = obj_value(e);
        return R::Ok;
    }
    if (is_tuple(a.args[0])) {
        out = a.args[0];
        return R::Ok;
    }
    ListObj *l = py_list_of(a.args[0]);
    if (!l)
        return R::Err;
    Root rl{ obj_value(l) };
    TupleObj *t = tuple_new(list_of(rl.v)->items.size());
    if (!t)
        return oom();
    for (usize i = 0; i < list_of(rl.v)->items.size(); i++)
        t->items()[i] = list_of(rl.v)->items[i];
    out = obj_value(t);
    return R::Ok;
}

// dict(m) for a class that writes keys(): every key, and m[key] for each.
// s[0] the dict, s[1] the bound keys, s[2] the bound __getitem__, s[3] the
// keys once they are in, s[4] and s[5] the call's keywords.
R dict_keys_step(ContObj *k, Value in)
{
    DictObj *d = static_cast<DictObj *>(k->s[0].obj());
    if (k->i == 0) {
        k->i = 1;
        return cont_call(k, k->s[1], Value(), 0);
    }
    if (k->i == 1) {
        // A view written in Python is walked by list(), which can.
        if (iter_needs_vm(in) || (is_inst(in) && !is_list(in))) {
            StrObj *ln = str_intern("list");
            Value fn;
            if (!ln || dict_get(builtins_dict(), obj_value(ln), fn) != R::Ok)
                return err_pending() ? R::Err : oom();
            return cont_call(k, fn, in);
        }
        ListObj *ks = py_list_of(in);
        if (!ks)
            return R::Err;
        k->s[3] = obj_value(ks);
        k->i    = 2;
    } else if (dict_set(d, list_of(k->s[3])->items[k->j - 1], in) != R::Ok) {
        return R::Err;
    }
    ListObj *ks = list_of(k->s[3]);
    if (k->j < ks->items.size())
        return cont_call(k, k->s[2], ks->items[k->j++]);
    TupleObj *names = static_cast<TupleObj *>(k->s[4].obj());
    TupleObj *vals  = static_cast<TupleObj *>(k->s[5].obj());
    for (usize i = 0; i < names->len; i++)
        if (dict_set(d, names->items()[i], vals->items()[i]) != R::Ok)
            return R::Err;
    // dict.update answers None; dict() answers the dict.
    return cont_done(k, k->s[6].is_nil() ? k->s[0] : value_none());
}

R b_dict(const CallArgs &a, Value &out)
{
    if (a.nargs > 1)
        return err_set("TypeError", "dict() takes at most one positional argument");
    if (a.nargs == 1 && is_frame_locals(a.args[0])) {
        Root d{ frame_locals_dict(a.args[0]) };
        if (d.v.is_nil())
            return R::Err;
        CallArgs b = a;
        b.args     = &d.v;
        return b_dict(b, out);
    }
    // A mapping proxy is the mapping it shows.
    if (a.nargs == 1 && is_mappingproxy(a.args[0])) {
        Root inner{ mappingproxy_inner(a.args[0]) };
        CallArgs b = a;
        b.args     = &inner.v;
        return b_dict(b, out);
    }
    if (a.nargs && !is_anydict(method_self(a.args[0])) && type_has_py_special(a.args[0], "keys")) {
        Root src{ a.args[0] };
        Root keys{ type_special(src.v, "keys") };
        Root get{ type_special(src.v, "__getitem__") };
        if (keys.v.is_nil() || get.v.is_nil())
            return err_pending() ? R::Err : not_iterable(src.v);
        Root d{ obj_value(dict_new()) };
        TupleObj *n = tuple_new(a.nkw), *v = n ? tuple_new(a.nkw) : nullptr;
        if (d.v.is_nil() || !n || !v)
            return oom();
        for (u32 i = 0; i < a.nkw; i++) {
            n->items()[i] = a.kwnames[i];
            v->items()[i] = a.kwvals[i];
        }
        Root rn{ obj_value(n) }, rv{ obj_value(v) };
        Root kv{ cont_new(dict_keys_step) };
        if (kv.v.is_nil())
            return R::Err;
        ContObj *k = cont_of(kv.v);
        k->s[0]    = d.v;
        k->s[1]    = keys.v;
        k->s[2]    = get.v;
        k->s[4]    = rn.v;
        k->s[5]    = rv.v;
        out        = kv.v;
        return R::Ok;
    }
    if (parks(a, 0))
        return iter_park(a, 0, b_dict, out);
    DictObj *d = dict_new();
    if (!d)
        return oom();
    Root rd{ obj_value(d) };
    if (a.nargs) {
        // A dict subclass stands for the dict inside it, so dict(d) copies
        // the mapping rather than trying to walk it as pairs.
        Root src{ method_self(a.args[0]) };
        if (is_anydict(src.v)) {
            usize at = 0;
            Value k, v;
            while (table_next(static_cast<DictObj *>(src.v.obj())->t, at, k, v))
                if (dict_set(static_cast<DictObj *>(rd.v.obj()), k, v) != R::Ok)
                    return R::Err;
        } else {
            // Anything else is a sequence of key/value pairs.
            Root it{ py_iter(src.v) };
            if (it.v.is_nil())
                return R::Err;
            for (usize at = 0;; at++) {
                Root got;
                R r = py_next(it.v, got.v);
                if (r == R::Err)
                    return R::Err;
                if (r == R::NotImpl)
                    break;
                usize n = 0;
                Root key, val;
                if (py_len(got.v, n) != R::Ok)
                    return R::Err;
                if (n != 2) {
                    char t[24];
                    Buf<128> m;
                    m.put("dictionary update sequence element #")
                        .put(int_text(t, sizeof t, i64(at)));
                    m.put(" has length ").put(int_text(t, sizeof t, i64(n))).put("; 2 is required");
                    return err_set("ValueError", m.str());
                }
                // Pin the key: taking the value allocates.
                if (py_getitem(got.v, Value::of_int(0), key.v) != R::Ok ||
                    py_getitem(got.v, Value::of_int(1), val.v) != R::Ok)
                    return R::Err;
                if (dict_set(static_cast<DictObj *>(rd.v.obj()), key.v, val.v) != R::Ok)
                    return R::Err;
            }
        }
    }
    for (u32 k = 0; k < a.nkw; k++)
        if (dict_set(static_cast<DictObj *>(rd.v.obj()), a.kwnames[k], a.kwvals[k]) != R::Ok)
            return R::Err;
    out = rd.v;
    return R::Ok;
}

R b_set(const CallArgs &a, Value &out)
{
    if (!args_only(a, "set", 0, 1))
        return R::Err;
    if (parks(a, 0))
        return iter_park(a, 0, b_set, out);
    SetObj *s = set_new();
    if (!s)
        return oom();
    Root rs{ obj_value(s) };
    if (a.nargs) {
        Root it{ py_iter(a.args[0]) };
        if (it.v.is_nil())
            return R::Err;
        for (;;) {
            Root got;
            R r = py_next(it.v, got.v);
            if (r == R::Err)
                return R::Err;
            if (r == R::NotImpl)
                break;
            if (set_add(static_cast<SetObj *>(rs.v.obj()), got.v) != R::Ok)
                return R::Err;
        }
    }
    out = rs.v;
    return R::Ok;
}

R b_range(const CallArgs &a, Value &out)
{
    if (!args_only(a, "range", 1, 3))
        return R::Err;
    Value n[3] = { Value::of_int(0), Value::of_int(0), Value::of_int(1) };
    for (u32 i = 0; i < a.nargs; i++) {
        // An int subclass, an IntEnum member, stands for its value.
        Value v = a.args[i];
        if (is_inst(v) && is_intval(inst_of(v)->native)) {
            n[i] = inst_of(v)->native;
            continue;
        }
        if (!is_intval(a.args[i])) {
            Buf<96> m;
            m.put('\'')
                .put(type_name(a.args[i]))
                .put("' object cannot be interpreted as an integer");
            return err_set("TypeError", m.str());
        }
        n[i] = a.args[i];
    }
    if (a.nargs == 1) {
        n[1] = n[0];
        n[0] = Value::of_int(0);
    }
    out = range_new_ints(n[0], n[1], n[2]);
    return out.is_nil() ? R::Err : R::Ok;
}

// ------------------------------------------------------ ordering the results

enum : u32 { KW_KEY = 1 << 0, KW_REVERSE = 1 << 1, KW_DEFAULT = 1 << 2 };

// The keyword arguments sorted, min and max take. `key` stays Nil when it is
// absent or None, which is the case that needs no callback at all.
R take_kw(const CallArgs &a, Str who, u32 allow, Value &key, bool &rev, Value &dflt, bool &has)
{
    for (u32 i = 0; i < a.nkw; i++) {
        Str n = is_str(a.kwnames[i]) ? str_of(a.kwnames[i])->str() : Str();
        if (n == "key" && (allow & KW_KEY)) {
            if (!is_none(a.kwvals[i]))
                key = a.kwvals[i];
        } else if (n == "reverse" && (allow & KW_REVERSE)) {
            rev = py_truth(a.kwvals[i]);
        } else if (n == "default" && (allow & KW_DEFAULT)) {
            dflt = a.kwvals[i];
            has  = true;
        } else {
            Buf<96> b;
            b.put(who).put("() got an unexpected keyword argument '").put(n).put("'");
            return err_set("TypeError", b.str());
        }
    }
    return R::Ok;
}

} // namespace

// Stable, bottom-up and iterative: a recursive sort is a risk on a 128 KiB
// stack, and Python's sort is stable.
R sort_idx(const Vec<Value> &keys, Vec<u32> &idx, bool rev)
{
    Vec<u32> tmp;
    for (usize i = 0; i < idx.size(); i++)
        if (!tmp.push(0))
            return oom();
    for (usize w = 1; w < idx.size(); w *= 2) {
        for (usize lo = 0; lo < idx.size(); lo += 2 * w) {
            usize mid = lo + w < idx.size() ? lo + w : idx.size();
            usize hi  = lo + 2 * w < idx.size() ? lo + 2 * w : idx.size();
            usize i = lo, j = mid, o = lo;
            while (i < mid && j < hi) {
                // The left run wins a tie, which is what makes it stable.
                bool take = false;
                if (py_cmp(keys[idx[j]], keys[idx[i]], rev ? Cmp::Gt : Cmp::Lt, take) != R::Ok)
                    return R::Err;
                tmp[o++] = take ? idx[j++] : idx[i++];
            }
            while (i < mid)
                tmp[o++] = idx[i++];
            while (j < hi)
                tmp[o++] = idx[j++];
        }
        for (usize i = 0; i < idx.size(); i++)
            idx[i] = tmp[i];
    }
    return R::Ok;
}

namespace {

// s[0] values, s[1] the key function, s[2] their keys, s[3] whether to reverse.
R sort_finish(ContObj *k)
{
    ListObj *vals = list_of(k->s[0]);
    ListObj *keys = list_of(k->s[2]);
    if (cmp_any_python(keys->items, true)) {
        Value c = cmp_sort(k->s[0], k->s[2], is_true(k->s[3]), false);
        return c.is_nil() ? R::Err : cont_done(k, c);
    }
    Vec<u32> idx;
    for (usize i = 0; i < vals->items.size(); i++)
        if (!idx.push(u32(i)))
            return oom();
    if (sort_idx(keys->items, idx, is_true(k->s[3])) != R::Ok)
        return R::Err;

    ListObj *sorted = list_new();
    if (!sorted)
        return oom();
    Root rs{ obj_value(sorted) };
    for (usize i = 0; i < idx.size(); i++)
        if (!list_push(list_of(rs.v), list_of(k->s[0])->items[idx[i]]))
            return oom();
    return cont_done(k, rs.v);
}

// One key per value, the function called once each; then the sort itself,
// which no longer needs Python.
R sort_step(ContObj *k, Value in)
{
    ListObj *vals = list_of(k->s[0]);
    if (k->i > 0 && !list_push(list_of(k->s[2]), in))
        return oom();
    if (k->i < vals->items.size())
        return cont_call(k, k->s[1], vals->items[k->i++]);
    return sort_finish(k);
}

R b_sorted(const CallArgs &a, Value &out)
{
    if (a.nargs != 1)
        return err_set("TypeError", "sorted() takes exactly one positional argument");
    if (parks(a, 0))
        return iter_park(a, 0, b_sorted, out);
    Root key, dflt;
    bool rev = false, has = false;
    if (take_kw(a, "sorted", KW_KEY | KW_REVERSE, key.v, rev, dflt.v, has) != R::Ok)
        return R::Err;

    ListObj *l = py_list_of(a.args[0]);
    if (!l)
        return R::Err;
    Root rl{ obj_value(l) };

    if (key.v.is_nil() && cmp_any_python(list_of(rl.v)->items, true)) {
        out = cmp_sort(rl.v, Value(), rev, false);
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (key.v.is_nil()) {
        Vec<u32> idx;
        for (usize i = 0; i < list_of(rl.v)->items.size(); i++)
            if (!idx.push(u32(i)))
                return oom();
        if (sort_idx(list_of(rl.v)->items, idx, rev) != R::Ok)
            return R::Err;
        ListObj *s = list_new();
        if (!s)
            return oom();
        Root rs{ obj_value(s) };
        for (usize i = 0; i < idx.size(); i++)
            if (!list_push(list_of(rs.v), list_of(rl.v)->items[idx[i]]))
                return oom();
        out = rs.v;
        return R::Ok;
    }

    Root kv{ cont_new(sort_step) };
    if (kv.v.is_nil())
        return R::Err;
    ListObj *keys = list_new();
    if (!keys)
        return oom();
    ContObj *k = cont_of(kv.v);
    k->s[0]    = rl.v;
    k->s[1]    = key.v;
    k->s[2]    = obj_value(keys);
    k->s[3]    = value_bool(rev);
    out        = kv.v;
    return R::Ok;
}

// ------------------------------------------------------------- reductions

// s[0] the candidates, s[1] the key function, s[2] the best so far, s[3] its
// key; j is set when the smallest is wanted.
R fold_step(ContObj *k, Value in)
{
    ListObj *xs = list_of(k->s[0]);
    if (k->i > 0 && !list_push(list_of(k->s[2]), in))
        return oom();
    if (k->i < xs->items.size())
        return cont_call(k, k->s[1], xs->items[k->i++]);

    // Every key is in hand. A key that compares in Python makes the fold
    // itself a continuation; anything else is a scan with no call in it.
    Vec<Value> &keys = list_of(k->s[2])->items;
    if (cmp_any_python(keys, true)) {
        Value c = cmp_fold(k->s[0], k->s[2], k->j != 0);
        return c.is_nil() ? R::Err : cont_done(k, c);
    }
    usize best = 0;
    for (usize i = 1; i < keys.size(); i++) {
        bool better = false;
        if (py_cmp(keys[i], keys[best], k->j ? Cmp::Lt : Cmp::Gt, better) != R::Ok)
            return R::Err;
        if (better)
            best = i;
    }
    return cont_done(k, best < xs->items.size() ? xs->items[best] : value_none());
}

// min and max: one iterable, or two or more candidates spelled out.
R fold(const CallArgs &a, bool least, Str who, Value &out)
{
    Root key, dflt;
    bool rev = false, has = false;
    if (take_kw(a, who, KW_KEY | KW_DEFAULT, key.v, rev, dflt.v, has) != R::Ok)
        return R::Err;
    if (!a.nargs)
        return err_set2("TypeError", "expected at least one argument", who);

    Root rl;
    if (a.nargs == 1) {
        ListObj *l = py_list_of(a.args[0]);
        if (!l)
            return R::Err;
        rl = obj_value(l);
    } else {
        ListObj *l = list_new();
        if (!l)
            return oom();
        rl = obj_value(l);
        for (u32 i = 0; i < a.nargs; i++)
            if (!list_push(list_of(rl.v), a.args[i]))
                return oom();
    }

    Vec<Value> &xs = list_of(rl.v)->items;
    if (xs.empty()) {
        if (!has)
            return err_set2("ValueError", "arg is an empty sequence", who);
        out = dflt.v;
        return R::Ok;
    }

    if (key.v.is_nil() && cmp_any_python(xs, true)) {
        out = cmp_fold(rl.v, Value(), least);
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (key.v.is_nil()) {
        Root best{ xs[0] };
        for (usize i = 1; i < xs.size(); i++) {
            bool better = false;
            if (py_cmp(xs[i], best.v, least ? Cmp::Lt : Cmp::Gt, better) != R::Ok)
                return R::Err;
            if (better)
                best = xs[i];
        }
        out = best.v;
        return R::Ok;
    }

    Root kv{ cont_new(fold_step) };
    if (kv.v.is_nil())
        return R::Err;
    ListObj *keys = list_new();
    if (!keys)
        return oom();
    ContObj *k = cont_of(kv.v);
    k->s[0]    = rl.v;
    k->s[1]    = key.v;
    k->s[2]    = obj_value(keys);
    k->j       = least ? 1 : 0;
    out        = kv.v;
    return R::Ok;
}

R b_min(const CallArgs &a, Value &out)
{
    if (a.nargs == 1 && parks(a, 0))
        return iter_park(a, 0, b_min, out);
    return fold(a, true, "min", out);
}

R b_max(const CallArgs &a, Value &out)
{
    if (a.nargs == 1 && parks(a, 0))
        return iter_park(a, 0, b_max, out);
    return fold(a, false, "max", out);
}

// Neumaier's running sum, which is what sum() keeps for floats.
struct Sum {
    f64 hi, lo;

    void add(f64 x)
    {
        f64 t = hi + x;
        lo += fabs(hi) >= fabs(x) ? (hi - t) + x : (x - t) + hi;
        hi = t;
    }

    // The compensation is left out when it would turn an overflow into a NaN.
    f64 value() const { return lo != 0 && isfinite(lo) ? hi + lo : hi; }
};

R b_sum(const CallArgs &a, Value &out)
{
    if (!args_only(a, "sum", 1, 2))
        return R::Err;
    if (parks(a, 0))
        return iter_park(a, 0, b_sum, out);
    Root acc{ a.nargs > 1 ? a.args[1] : Value::of_int(0) };
    Root it{ py_iter(a.args[0]) };
    if (it.v.is_nil())
        return R::Err;
    // A float or complex total is summed with compensation, as 3.12 does, and
    // only becomes an object again when an item is something else.
    bool comp = false;
    Sum re, im;
    for (;;) {
        Root got;
        R r = py_next(it.v, got.v);
        if (r == R::Err)
            return R::Err;
        if (r == R::NotImpl)
            break;
        if (!comp && (is_float(acc.v) || is_complex(acc.v))) {
            re   = { is_float(acc.v) ? float_of(acc.v) : complex_of(acc.v)->re, 0 };
            im   = { is_complex(acc.v) ? complex_of(acc.v)->im : 0, 0 };
            comp = true;
        }
        if (comp) {
            bool cx = is_complex(acc.v);
            if (is_float(got.v)) {
                re.add(float_of(got.v));
                continue;
            }
            if (cx && is_complex(got.v)) {
                re.add(complex_of(got.v)->re);
                im.add(complex_of(got.v)->im);
                continue;
            }
            if (is_intval(got.v)) {
                f64 x = int_to_f64(got.v);
                if (isinf(x))
                    return err_set("OverflowError", "int too large to convert to float");
                re.add(x);
                continue;
            }
            acc  = cx ? complex_new(re.value(), im.value()) : float_new(re.value());
            comp = false;
            if (acc.v.is_nil())
                return R::Err;
        }
        Value next;
        if (binop_call(acc.v, got.v, Op::Add, next) != R::Ok)
            return R::Err;
        if (is_cont(next))
            return fold_rest(it.v, next, Op::Add, out);
        acc = next;
    }
    if (comp)
        acc = is_complex(acc.v) ? complex_new(re.value(), im.value()) : float_new(re.value());
    out = acc.v;
    return out.is_nil() ? R::Err : R::Ok;
}

// all() and any(), one item at a time: they stop at the first answer, and the
// rest is never run. s[0] the iterable, then its iterator; s[1] its __next__,
// Nil for a native one. j is the item that decides, bit 8 a __len__ pending.
R every_step(ContObj *k, Value in)
{
    bool want = k->j & 1;
    switch (k->i) {
    case 0: {
        k->i = 1;
        Root sp{ iter_special(k->s[0]) };
        if (!sp.v.is_nil())
            return cont_call(k, sp.v, Value(), 0);
        if (err_pending())
            return R::Err;
        in = py_iter(k->s[0]);
        if (in.is_nil())
            return R::Err;
    }
        [[fallthrough]];
    case 1:
        k->s[0] = in;
        if (iter_needs_vm(in)) {
            k->s[1] = next_special(in);
            if (k->s[1].is_nil()) {
                if (err_pending())
                    return R::Err;
                Buf<96> b;
                b.put("iter() returned non-iterator of type '").put(type_name(in)).put("'");
                return err_set("TypeError", b.str());
            }
        }
        in = Value();
        break;
    case 2:
        // An item from __next__, or Nil at its end.
        if (in.is_nil())
            return cont_done(k, value_bool(!want));
        break;
    default: {
        // What an item's __bool__ or __len__ said.
        bool yes = false;
        if (truth_answer(in, k->j & 0x100, yes) != R::Ok)
            return R::Err;
        if (yes == want)
            return cont_done(k, value_bool(want));
        in = Value();
        break;
    }
    }
    for (;;) {
        if (in.is_nil()) {
            if (!k->s[1].is_nil()) {
                k->i        = 2;
                k->catching = CATCH_STOP;
                return cont_call(k, k->s[1], Value(), 0);
            }
            Root got;
            R r = py_next(k->s[0], got.v);
            if (r == R::Err)
                return R::Err;
            if (r == R::NotImpl)
                return cont_done(k, value_bool(!want));
            in = got.v;
        }
        Root item{ in };
        bool len = false;
        Root m{ truth_special(item.v, len) };
        if (!m.v.is_nil()) {
            k->i        = 3;
            k->j        = (k->j & 1) | (len ? 0x100 : 0);
            k->catching = CATCH_NONE;
            return cont_call(k, m.v, Value(), 0);
        }
        if (err_pending())
            return R::Err;
        bool yes = py_truth(item.v);
        if (err_pending())
            return R::Err;
        if (yes == want)
            return cont_done(k, value_bool(want));
        in = Value();
    }
}

R every_of(const CallArgs &a, Str who, bool want, Value &out)
{
    if (!args_only(a, who, 1, 1))
        return R::Err;
    Root it{ a.args[0] };
    Root kv{ cont_new(every_step) };
    if (kv.v.is_nil())
        return R::Err;
    cont_of(kv.v)->s[0] = it.v;
    cont_of(kv.v)->j    = want;
    out                 = kv.v;
    return R::Ok;
}

R b_all(const CallArgs &a, Value &out)
{
    return every_of(a, "all", false, out);
}

R b_any(const CallArgs &a, Value &out)
{
    return every_of(a, "any", true, out);
}

R b_enumerate(const CallArgs &a, Value &out)
{
    Root seq, start{ Value::of_int(0) };
    if (a.nargs > 2)
        return err_set("TypeError", "enumerate() takes from 1 to 2 arguments");
    if (parks(a, 0))
        return iter_park(a, 0, b_enumerate, out);
    if (a.nargs > 0)
        seq = a.args[0];
    if (a.nargs > 1)
        start = a.args[1];
    for (u32 i = 0; i < a.nkw; i++) {
        Str n = is_str(a.kwnames[i]) ? str_of(a.kwnames[i])->str() : Str();
        if (n == "iterable")
            seq = a.kwvals[i];
        else if (n == "start")
            start = a.kwvals[i];
        else
            return err_set2("TypeError", "enumerate() got an unexpected keyword argument", n);
    }
    if (seq.v.is_nil())
        return err_set("TypeError", "enumerate() missing a required argument: 'iterable'");
    i64 n = 0;
    if (!as_index(start.v, n))
        return err_set2("TypeError", "enumerate() start must be an integer", type_name(start.v));
    out = enum_iter(seq.v, n);
    return out.is_nil() ? R::Err : R::Ok;
}

R b_iter(const CallArgs &a, Value &out)
{
    if (!args_only(a, "iter", 1, 2))
        return R::Err;
    if (a.nargs == 2) {
        if (!py_callable(a.args[0]))
            return err_set("TypeError", "iter(v, w): v must be callable");
        out = calliter_new(a.args[0], a.args[1]);
        return out.is_nil() ? R::Err : R::Ok;
    }
    // A class's own __iter__, or the walk over its __getitem__.
    Root m{ iter_special(a.args[0]) };
    if (!m.v.is_nil()) {
        TupleObj *none = tuple_new(0);
        if (!none)
            return oom();
        out = attr_invoke(m.v, obj_value(none));
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (err_pending())
        return R::Err;
    out = py_iter(a.args[0]);
    return out.is_nil() ? R::Err : R::Ok;
}

// next() over a generator. s[0] is the bound __next__, s[1] the default.
R next1_step(ContObj *k, Value in)
{
    if (k->i++ == 0)
        return cont_call(k, k->s[0], Value(), 0);
    return cont_done(k, in.is_nil() ? k->s[1] : in);
}

R b_next(const CallArgs &a, Value &out)
{
    if (!args_only(a, "next", 1, 2))
        return R::Err;
    if (iter_needs_vm(a.args[0])) {
        Root m{ next_special(a.args[0]) };
        if (m.v.is_nil())
            return R::Err;
        Root d{ a.nargs > 1 ? a.args[1] : Value() };
        Root kv{ cont_new(next1_step) };
        if (kv.v.is_nil())
            return R::Err;
        ContObj *k = cont_of(kv.v);
        k->s[0]    = m.v;
        k->s[1]    = d.v;
        // Without a default the StopIteration is the answer. With one, it
        // is caught and the default stands.
        k->catching = a.nargs > 1 ? CATCH_STOP : CATCH_NONE;
        out         = kv.v;
        return R::Ok;
    }
    R r = py_next(a.args[0], out);
    if (r != R::NotImpl)
        return r;
    if (a.nargs > 1) {
        out = a.args[1];
        return R::Ok;
    }
    Value e = exc_new(exc_find("StopIteration"), Value());
    return e.is_nil() ? R::Err : err_set_value(e);
}

// ------------------------------------------------------------- attributes

// getattr(o, name[, default]), and hasattr over the same lookup. A property
// getter is Python, so both of them can suspend.
R attr_of(const CallArgs &a, Str who, bool want_bool, Value &out)
{
    if (a.nkw || a.nargs < 2 || a.nargs > (want_bool ? 2u : 3u))
        return err_set2("TypeError", "wrong number of arguments", who);
    if (!is_str(a.args[1]))
        return err_set2("TypeError", "attribute name must be a string", type_name(a.args[1]));

    StrObj *n = str_intern(str_of(a.args[1])->str());
    if (!n)
        return oom();
    // An AttributeError is caught only where there is something to answer with.
    bool guard = want_bool || a.nargs > 2;
    Root got;
    Got g = guard ? py_attr_opt(a.args[0], n, got.v, a.nargs > 2 ? a.args[2] : Value(), want_bool)
                  : py_attr(a.args[0], n, got.v);
    switch (g) {
    case Got::Error:
        return R::Err;
    case Got::Ok:
    case Got::Call:
        out = got.v;
        return R::Ok;
    case Got::Missing:
        break;
    }
    if (want_bool) {
        out = value_bool(false);
        return R::Ok;
    }
    if (a.nargs > 2) {
        out = a.args[2];
        return R::Ok;
    }
    return attr_missing(a.args[0], str_of(a.args[1])->str());
}

R b_getattr(const CallArgs &a, Value &out)
{
    return attr_of(a, "getattr", false, out);
}

R b_hasattr(const CallArgs &a, Value &out)
{
    return attr_of(a, "hasattr", true, out);
}

R b_setattr(const CallArgs &a, Value &out)
{
    if (!args_only(a, "setattr", 3, 3))
        return R::Err;
    if (!is_str(a.args[1]))
        return err_set2("TypeError", "attribute name must be a string", type_name(a.args[1]));
    StrObj *n = str_intern(str_of(a.args[1])->str());
    if (!n)
        return oom();
    Value fn;
    if (attr_store(a.args[0], n, a.args[2], fn) != R::Ok)
        return R::Err;
    // A __set__ or a __setattr__ is Python, so setattr() suspends too.
    out = fn.is_nil() ? value_none() : fn;
    return R::Ok;
}

R b_delattr(const CallArgs &a, Value &out)
{
    if (!args_only(a, "delattr", 2, 2))
        return R::Err;
    if (!is_str(a.args[1]))
        return err_set2("TypeError", "attribute name must be a string", type_name(a.args[1]));
    StrObj *n = str_intern(str_of(a.args[1])->str());
    if (!n)
        return oom();
    Value fn;
    if (attr_delete(a.args[0], n, fn) != R::Ok)
        return R::Err;
    out = fn.is_nil() ? value_none() : fn;
    return R::Ok;
}

R b_callable(const CallArgs &a, Value &out)
{
    if (!args_only(a, "callable", 1, 1))
        return R::Err;
    out = value_bool(py_callable(a.args[0]));
    return R::Ok;
}

R b_hash(const CallArgs &a, Value &out)
{
    if (!args_only(a, "hash", 1, 1))
        return R::Err;
    if (type_unhashable(a.args[0]))
        return err_unhashable(a.args[0]);
    out = one_special(a.args[0], "__hash__", WANT_HASH);
    if (!out.is_nil())
        return R::Ok;
    if (err_pending())
        return R::Err;
    u32 h = 0;
    if (py_hash(a.args[0], h) != R::Ok)
        return R::Err;
    out = int_from_i64(i32(h));
    return out.is_nil() ? R::Err : R::Ok;
}

R b_id(const CallArgs &a, Value &out)
{
    if (!args_only(a, "id", 1, 1))
        return R::Err;
    out = int_from_i64(a.args[0].is_obj() ? i64(usize(a.args[0].obj())) : i64(a.args[0].w));
    return out.is_nil() ? R::Err : R::Ok;
}

// --------------------------------------------------- arithmetic with a shape

// divmod(a, b) over __divmod__ and __rdivmod__ written in Python. s[0] and
// s[1] the two calls to try, s[2] and s[3] their arguments, s[4]/s[5] a and b.
R divmod_step(ContObj *k, Value in)
{
    u32 phase = k->i++;
    if (phase == 0 && !k->s[0].is_nil())
        return cont_call(k, k->s[0], k->s[2]);
    if (phase == 0)
        k->i = 2;
    if (phase > 0 && !is_notimpl(in))
        return cont_done(k, in);
    if (k->i == 2 && !k->s[1].is_nil()) {
        k->i = 3;
        return cont_call(k, k->s[1], k->s[3]);
    }
    Buf<128> m;
    m.put("unsupported operand type(s) for divmod(): '").put(type_name(k->s[4]));
    m.put("' and '").put(type_name(k->s[5])).put('\'');
    return err_set("TypeError", m.str());
}

R b_divmod(const CallArgs &a, Value &out)
{
    if (!args_only(a, "divmod", 2, 2))
        return R::Err;
    if (type_has_py_special(a.args[0], "__divmod__") ||
        type_has_py_special(a.args[1], "__rdivmod__")) {
        Root l{ type_has_py_special(a.args[0], "__divmod__") ? type_special(a.args[0], "__divmod__")
                                                             : Value() };
        Root r{ type_has_py_special(a.args[1], "__rdivmod__")
                    ? type_special(a.args[1], "__rdivmod__")
                    : Value() };
        Root kv{ cont_new(divmod_step) };
        if (kv.v.is_nil())
            return R::Err;
        ContObj *k = cont_of(kv.v);
        k->s[0]    = l.v;
        k->s[1]    = r.v;
        k->s[2]    = a.args[1];
        k->s[3]    = a.args[0];
        k->s[4]    = a.args[0];
        k->s[5]    = a.args[1];
        out        = kv.v;
        return R::Ok;
    }
    Root q, r;
    if (py_binop(a.args[0], a.args[1], Op::FloorDiv, q.v) != R::Ok ||
        py_binop(a.args[0], a.args[1], Op::Mod, r.v) != R::Ok)
        return R::Err;
    TupleObj *t = tuple_new(2);
    if (!t)
        return oom();
    t->items()[0] = q.v;
    t->items()[1] = r.v;
    out           = obj_value(t);
    return R::Ok;
}

// To the nearest, ties to even. C's rint does not promise that.
f64 round_half_even(f64 v)
{
    f64 down = floor(v);
    f64 frac = v - down;
    if (frac > 0.5)
        return down + 1;
    if (frac < 0.5)
        return down;
    return fmod(down, 2.0) == 0 ? down : down + 1;
}

R b_round(const CallArgs &a, Value &out)
{
    if (!args_only(a, "round", 1, 2))
        return R::Err;
    // A class's own __round__, with ndigits when that was given.
    R own = R::Ok;
    if (answer_special(a.args[0], "__round__",
                       a.nargs > 1 && !is_none(a.args[1]) ? a.args[1] : Value(), out, own))
        return own;
    i64 digits = 0;
    if (a.nargs > 1 && !is_none(a.args[1]) && !as_index(a.args[1], digits))
        return err_set2("TypeError", "round() ndigits must be an integer", type_name(a.args[1]));
    bool to_int = a.nargs < 2 || is_none(a.args[1]);

    if (is_intval(a.args[0])) {
        // An int rounds to itself at any precision above zero, and stays an
        // int below it -- exactly, whatever its width.
        if (to_int || digits >= 0) {
            out = is_bool(a.args[0]) ? Value::of_int(is_true(a.args[0]) ? 1 : 0) : a.args[0];
            return R::Ok;
        }
        Root scale{ Value::of_int(10) }, exp{ int_from_i64(-digits) };
        if (scale.v.is_nil() || exp.v.is_nil())
            return R::Err;
        if (int_power(scale.v, exp.v, Value(), scale.v) != R::Ok)
            return R::Err;
        Root q, r;
        if (py_binop(a.args[0], scale.v, Op::FloorDiv, q.v) != R::Ok ||
            py_binop(a.args[0], scale.v, Op::Mod, r.v) != R::Ok)
            return R::Err;
        // Half goes to even, which is the rule for a float too.
        Root twice;
        if (py_binop(r.v, Value::of_int(2), Op::Mul, twice.v) != R::Ok)
            return R::Err;
        bool up = false, tie = false, odd = false;
        if (py_cmp(twice.v, scale.v, Cmp::Gt, up) != R::Ok || py_eq(twice.v, scale.v, tie) != R::Ok)
            return R::Err;
        if (tie) {
            Root bit;
            if (py_binop(q.v, Value::of_int(1), Op::And, bit.v) != R::Ok)
                return R::Err;
            odd = bit.v == Value::of_int(1);
        }
        if (up || (tie && odd))
            if (py_binop(q.v, Value::of_int(1), Op::Add, q.v) != R::Ok)
                return R::Err;
        return py_binop(q.v, scale.v, Op::Mul, out);
    }
    f64 v = 0;
    if (!as_number(a.args[0], v)) {
        Buf<128> b;
        b.put("type ").put(type_name(a.args[0])).put(" doesn't define __round__ method");
        return err_set("TypeError", b.str());
    }
    if (to_int) {
        out = int_from_i64(i64(round_half_even(v)));
        return out.is_nil() ? R::Err : R::Ok;
    }
    // Round the decimal, not the value scaled by a power of ten: 2.675 * 100
    // is exactly 267.5 in binary and would round up, where 2.675 rounds down.
    // Format to `digits` places and read it back, as CPython does.
    f64 mag = v < 0 ? -v : v;
    if (digits >= 0 && digits < 18 && mag < 1e16) {
        char tmp[64];
        Option<f64> back = parse_f64(fmt_f64(tmp, sizeof tmp, v, i32(digits), 'f'));
        if (back.has_value()) {
            out = float_new(back.value());
            return out.is_nil() ? R::Err : R::Ok;
        }
    }
    f64 scale = pow(10.0, f64(digits));
    if (isinf(scale) || scale == 0 || mag >= 1e16) {
        out = float_new(v);
        return out.is_nil() ? R::Err : R::Ok;
    }
    out = float_new(round_half_even(v * scale) / scale);
    return out.is_nil() ? R::Err : R::Ok;
}

// a⁻¹ modulo n, n positive: extended Euclid, as CPython's long_invmod.
R int_invmod(Value a, Value n, Value &out)
{
    Root b{ Value::of_int(1) }, c{ Value::of_int(0) }, x, y{ n }, q, r, t;
    if (py_binop(a, n, Op::Mod, x.v) != R::Ok)
        return R::Err;
    for (;;) {
        bool zero = false;
        if (py_eq(y.v, Value::of_int(0), zero) != R::Ok)
            return R::Err;
        if (zero)
            break;
        if (py_binop(x.v, y.v, Op::FloorDiv, q.v) != R::Ok ||
            py_binop(x.v, y.v, Op::Mod, r.v) != R::Ok)
            return R::Err;
        x = y.v;
        y = r.v;
        if (py_binop(q.v, c.v, Op::Mul, t.v) != R::Ok || py_binop(b.v, t.v, Op::Sub, t.v) != R::Ok)
            return R::Err;
        b = c.v;
        c = t.v;
    }
    bool one = false;
    if (py_eq(x.v, Value::of_int(1), one) != R::Ok)
        return R::Err;
    if (!one)
        return err_set("ValueError", "base is not invertible for the given modulus");
    return py_binop(b.v, n, Op::Mod, out);
}

R b_pow(const CallArgs &a, Value &out)
{
    if (!args_only(a, "pow", 2, 3))
        return R::Err;
    if (a.nargs < 3 || is_none(a.args[2]))
        return py_binop(a.args[0], a.args[1], Op::Pow, out);

    if (!is_intval(a.args[0]) || !is_intval(a.args[1]) || !is_intval(a.args[2]))
        return err_set("TypeError", "pow() 3rd argument not allowed unless all arguments are ints");
    if (!int_is_neg(a.args[1]))
        return int_power(a.args[0], a.args[1], a.args[2], out);
    // A negative exponent is the modular inverse raised to its magnitude.
    bool zero = false;
    if (py_eq(a.args[2], Value::of_int(0), zero) != R::Ok)
        return R::Err;
    if (zero)
        return err_set("ValueError", "pow() 3rd argument cannot be 0");
    Root m, e, inv;
    if (py_binop(Value::of_int(0), a.args[2], Op::Sub, m.v) != R::Ok ||
        py_binop(Value::of_int(0), a.args[1], Op::Sub, e.v) != R::Ok)
        return R::Err;
    if (!int_is_neg(a.args[2]))
        m = a.args[2];
    bool unit = false;
    if (py_eq(m.v, Value::of_int(1), unit) != R::Ok)
        return R::Err;
    if (unit) {
        out = Value::of_int(0);
        return R::Ok;
    }
    if (int_invmod(a.args[0], m.v, inv.v) != R::Ok)
        return R::Err;
    return int_power(inv.v, e.v, a.args[2], out);
}

// ----------------------------------------------------------- the iterators

// reversed() of a class of the program's own: its __reversed__, or its
// __len__ and __getitem__ read from the end, eagerly, into a list. s[0] the
// object, s[1] __getitem__, s[2] the list; x[0] the next index.
R rev_step(ContObj *k, Value in)
{
    if (k->i == 0) {
        k->i = 1;
        return cont_call(k, k->s[1], Value(), 0);
    }
    if (k->i == 1) {
        i64 n = 0;
        if (!as_index(in, n))
            return err_set2("TypeError", "'__len__' must return an integer", type_name(in));
        if (n < 0)
            return err_set("ValueError", "__len__() should return >= 0");
        ListObj *l = list_new();
        if (!l)
            return oom();
        k->s[2] = obj_value(l);
        k->x[0] = n - 1;
        k->i    = 2;
        Root get{ type_special(k->s[0], "__getitem__") };
        if (get.v.is_nil())
            return err_pending() ? R::Err : err_set("TypeError", "object is not reversible");
        k->s[1] = get.v;
    } else if (!list_push(list_of(k->s[2]), in)) {
        return oom();
    }
    if (k->x[0] < 0) {
        Value it = py_iter(k->s[2]);
        return it.is_nil() ? R::Err : cont_done(k, it);
    }
    Value idx = int_from_i64(k->x[0]--);
    return idx.is_nil() ? R::Err : cont_call(k, k->s[1], idx);
}

R reversed_python(Value obj, Value &out, bool &done)
{
    done = false;
    if (!is_inst(obj) || !inst_of(obj)->native.is_nil())
        return R::Ok;
    Root ro{ obj };
    Root rev{ type_special(ro.v, "__reversed__") };
    Root len;
    if (rev.v.is_nil() || is_none(rev.v)) {
        len = type_special(ro.v, "__len__");
        if (err_pending())
            return R::Err;
        if (!rev.v.is_nil() || len.v.is_nil() || type_special(ro.v, "__getitem__").is_nil()) {
            Buf<128> m;
            m.put('\'').put(type_name(ro.v)).put("' object is not reversible");
            return err_set("TypeError", m.str());
        }
    }
    Root kv{ cont_new(rev_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = ro.v;
    if (!rev.v.is_nil()) {
        // Its answer is the iterator.
        k->i    = 3;
        k->step = [](ContObj *c, Value in) -> R {
            if (c->i == 3) {
                c->i = 4;
                return cont_call(c, c->s[1], Value(), 0);
            }
            return cont_done(c, in);
        };
        k->s[1] = rev.v;
    } else {
        k->s[1] = len.v;
    }
    out  = kv.v;
    done = true;
    return R::Ok;
}

R b_reversed(const CallArgs &a, Value &out)
{
    if (!args_only(a, "reversed", 1, 1))
        return R::Err;
    bool done = false;
    if (reversed_python(a.args[0], out, done) != R::Ok)
        return R::Err;
    if (done)
        return R::Ok;
    // A FrameLocalsProxy is reversed as its keys.
    if (is_frame_locals(a.args[0])) {
        Root d{ frame_locals_dict(a.args[0]) };
        ListObj *keys = d.v.is_nil() ? nullptr : py_list_of(d.v);
        if (!keys)
            return R::Err;
        out = reversed_new(obj_value(keys));
        return out.is_nil() ? R::Err : R::Ok;
    }
    out = reversed_new(a.args[0]);
    return out.is_nil() ? R::Err : R::Ok;
}

R b_zip(const CallArgs &a, Value &out)
{
    bool strict = false;
    for (u32 i = 0; i < a.nkw; i++) {
        if (str_of(a.kwnames[i])->str() != "strict")
            return err_set2("TypeError", "zip() got an unexpected keyword argument",
                            str_of(a.kwnames[i])->str());
        strict = py_truth(a.kwvals[i]);
    }
    for (u32 i = 0; i < a.nargs; i++)
        if (parks(a, i))
            return iter_park(a, i, b_zip, out);
    TupleObj *t = tuple_new(a.nargs);
    if (!t)
        return oom();
    Root rt{ obj_value(t) };
    for (u32 i = 0; i < a.nargs; i++) {
        Value it = py_iter(a.args[i]);
        if (it.is_nil())
            return R::Err;
        static_cast<TupleObj *>(rt.v.obj())->items()[i] = it;
    }
    out = zip_new(rt.v, strict);
    return out.is_nil() ? R::Err : R::Ok;
}

// map and filter call back from inside the iterator protocol, and py_next has
// no way to suspend. So both run the function over the whole input first and
// return an iterator on the result: eager where CPython is lazy. See README.md.
//
// s[0] the input, s[1] the function, s[2] what has been kept; j says filter.
R apply_step(ContObj *k, Value in)
{
    ListObj *xs = list_of(k->s[0]);
    if (k->i > 0) {
        Value item = xs->items[k->i - 1];
        if (!k->j) {
            if (!list_push(list_of(k->s[2]), in))
                return oom();
        } else if (py_truth(in)) {
            if (!list_push(list_of(k->s[2]), item))
                return oom();
        }
    }
    if (k->i < xs->items.size())
        return cont_call(k, k->s[1], xs->items[k->i++]);
    Value it = made_iter(k->s[2], k->j ? &filter_type : &map_type);
    return it.is_nil() ? R::Err : cont_done(k, it);
}

// Several iterables: map(f, a, b) calls f(x, y).
R map_many(const CallArgs &a, Value &out);

R b_map(const CallArgs &a, Value &out)
{
    if (a.nkw || a.nargs < 2)
        return err_set("TypeError", "map() must have at least two arguments");
    for (u32 i = 1; i < a.nargs; i++)
        if (parks(a, i))
            return iter_park(a, i, b_map, out);
    if (a.nargs > 2)
        return map_many(a, out);

    ListObj *xs = py_list_of(a.args[1]);
    if (!xs)
        return R::Err;
    Root rx{ obj_value(xs) };
    ListObj *kept = list_new();
    if (!kept)
        return oom();
    Root rk{ obj_value(kept) };
    Root kv{ cont_new(apply_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = rx.v;
    k->s[1]    = a.args[0];
    k->s[2]    = rk.v;
    k->j       = 0;
    out        = kv.v;
    return R::Ok;
}

// s[0] the tuples of arguments, s[1] the function, s[2] the results.
R map_step(ContObj *k, Value in)
{
    ListObj *xs = list_of(k->s[0]);
    if (k->i > 0 && !list_push(list_of(k->s[2]), in))
        return oom();
    if (k->i < xs->items.size())
        return cont_call_v(k, k->s[1], xs->items[k->i++]);
    Value it = made_iter(k->s[2], &map_type);
    return it.is_nil() ? R::Err : cont_done(k, it);
}

R map_many(const CallArgs &a, Value &out)
{
    // Every argument tuple first, so the step only calls.
    TupleObj *its = tuple_new(a.nargs - 1);
    if (!its)
        return oom();
    Root ri{ obj_value(its) };
    for (u32 i = 1; i < a.nargs; i++) {
        Value it = py_iter(a.args[i]);
        if (it.is_nil())
            return R::Err;
        static_cast<TupleObj *>(ri.v.obj())->items()[i - 1] = it;
    }
    ListObj *rows = list_new();
    if (!rows)
        return oom();
    Root rr{ obj_value(rows) };
    for (;;) {
        TupleObj *row = tuple_new(a.nargs - 1);
        if (!row)
            return oom();
        Root rw{ obj_value(row) };
        bool done = false;
        for (u32 i = 0; i + 1 < a.nargs && !done; i++) {
            Value got;
            R r = py_next(static_cast<TupleObj *>(ri.v.obj())->items()[i], got);
            if (r == R::Err)
                return R::Err;
            if (r == R::NotImpl)
                done = true;
            else
                static_cast<TupleObj *>(rw.v.obj())->items()[i] = got;
        }
        if (done)
            break;
        if (!list_push(list_of(rr.v), rw.v))
            return oom();
    }
    ListObj *kept = list_new();
    if (!kept)
        return oom();
    Root rk{ obj_value(kept) };
    Root kv{ cont_new(map_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = rr.v;
    k->s[1]    = a.args[0];
    k->s[2]    = rk.v;
    out        = kv.v;
    return R::Ok;
}

R b_filter(const CallArgs &a, Value &out)
{
    if (!args_only(a, "filter", 2, 2))
        return R::Err;
    if (parks(a, 1))
        return iter_park(a, 1, b_filter, out);
    ListObj *xs = py_list_of(a.args[1]);
    if (!xs)
        return R::Err;
    Root rx{ obj_value(xs) };
    ListObj *kept = list_new();
    if (!kept)
        return oom();
    Root rk{ obj_value(kept) };

    // filter(None, xs) keeps what is true and calls nothing.
    if (is_none(a.args[0])) {
        Vec<Value> &items = list_of(rx.v)->items;
        for (usize i = 0; i < items.size(); i++)
            if (py_truth(items[i]) && !list_push(list_of(rk.v), items[i]))
                return oom();
        out = made_iter(rk.v, &filter_type);
        return out.is_nil() ? R::Err : R::Ok;
    }
    Root kv{ cont_new(apply_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = rx.v;
    k->s[1]    = a.args[0];
    k->s[2]    = rk.v;
    k->j       = 1;
    out        = kv.v;
    return R::Ok;
}

// ------------------------------------------------------- the newer types

// What a codec written in Python made, as a bytearray.
R growable_step(ContObj *k, Value in)
{
    Str s;
    if (!bytes_like(in, s))
        return err_set2("TypeError", "encoder did not return bytes", type_name(in));
    Value v = bytearray_new(s);
    return v.is_nil() ? R::Err : cont_done(k, v);
}

R b_bytearray(const CallArgs &a, Value &out)
{
    if (a.nkw || a.nargs > 1) {
        Root made;
        if (encoded(a, "bytearray", made.v) != R::Ok)
            return R::Err;
        if (is_cont(made.v)) {
            // The codec's continuation runs first, then this one.
            Root kv{ cont_new(growable_step) };
            if (kv.v.is_nil())
                return R::Err;
            cont_of(made.v)->next = kv.v;
            out                   = made.v;
            return R::Ok;
        }
        out = bytearray_new(static_cast<BytesObj *>(made.v.obj())->str());
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (a.nargs == 1 && parks(a, 0))
        return iter_park(a, 0, b_bytearray, out);
    Str s;
    if (a.nargs == 1 && bytes_like(a.args[0], s)) {
        out = bytearray_new(s);
        return out.is_nil() ? R::Err : R::Ok;
    }
    // A count, a string with an encoding, or a sequence of octets: bytes'
    // rule, and the run is then made growable.
    Root made;
    if (b_bytes(a, made.v) != R::Ok)
        return R::Err;
    bytes_like(made.v, s);
    out = bytearray_new(s);
    return out.is_nil() ? R::Err : R::Ok;
}

R b_frozenset(const CallArgs &a, Value &out)
{
    if (!args_only(a, "frozenset", 0, 1))
        return R::Err;
    if (parks(a, 0))
        return iter_park(a, 0, b_frozenset, out);
    SetObj *s = frozenset_new();
    if (!s)
        return oom();
    Root rs{ obj_value(s) };
    if (a.nargs) {
        Root it{ py_iter(a.args[0]) };
        if (it.v.is_nil())
            return R::Err;
        for (;;) {
            Root got;
            R r = py_next(it.v, got.v);
            if (r == R::Err)
                return R::Err;
            if (r == R::NotImpl)
                break;
            if (set_add(set_at(rs.v), got.v) != R::Ok)
                return R::Err;
        }
    }
    out = rs.v;
    return R::Ok;
}

R b_memoryview(const CallArgs &a, Value &out)
{
    if (!args_only(a, "memoryview", 1, 1))
        return R::Err;
    out = memview_new(a.args[0]);
    return out.is_nil() ? R::Err : R::Ok;
}

R b_slice(const CallArgs &a, Value &out)
{
    if (!args_only(a, "slice", 1, 3))
        return R::Err;
    Value parts[3] = { value_none(), value_none(), value_none() };
    if (a.nargs == 1) {
        parts[1] = a.args[0];
    } else {
        for (u32 i = 0; i < a.nargs; i++)
            parts[i] = a.args[i];
    }
    out = slice_new(parts[0], parts[1], parts[2]);
    return out.is_nil() ? R::Err : R::Ok;
}

// --------------------------------------------------------------------- sys

// ------------------------------------------------- compile, eval and exec

// What a source may arrive as. CPython takes bytes here too.
bool source_text(Value v, Str &out)
{
    if (is_str(v)) {
        out = str_of(v)->str();
        // A lone surrogate is refused as encoding the source to UTF-8 would.
        if (has_surrogate(out)) {
            Value made;
            if (text_encode(v, Value(), Value(), made) != R::Ok)
                return false;
        }
        return true;
    }
    return bytes_like(v, out);
}

// compile()'s third argument.
bool compile_mode(Value v, CompileMode &out)
{
    Str m = is_str(v) ? str_of(v)->str() : Str();
    if (!is_str(v))
        return err_set2("TypeError", "compile() mode must be a string", type_name(v)), false;
    if (m == "exec")
        out = CompileMode::Exec;
    else if (m == "eval")
        out = CompileMode::Eval;
    else if (m == "single")
        out = CompileMode::Single;
    else
        return err_set("ValueError", "compile() mode must be 'exec', 'eval' or 'single'"), false;
    return true;
}

// bytes whose cookie names a codec written in Python: decode them first, then
// come back with the str.
R park_decode(const CallArgs &a, Str codec, R (*again)(const CallArgs &, Value &out), Value &out)
{
    err_clear();
    Root fn{ native_new("decode", lex_source_decode) };
    Root enc{ str_new(codec) };
    if (fn.v.is_nil() || enc.v.is_nil())
        return R::Err;
    return redo_with(a, 0, fn.v, a.args[0], enc.v, 2, again, out);
}

// Parse and compile. The Ast is a stack object, so the code object it leaves
// behind is what outlives this. `codec` takes the name of a codec the source
// has to be decoded with first, when that is what stopped it.
// `tree` asks for what PyCF_ONLY_AST answers rather than a code object.
// `flags` carries the other two codeop reads: DONT_IMPLY_DEDENT leaves a
// suite open at the end, and ALLOW_INCOMPLETE_INPUT reports a source that
// simply stopped short as _IncompleteInputError rather than SyntaxError.
Value compile_source(Value src, Str filename, CompileMode mode, String *codec = nullptr,
                     bool tree = false, i64 flags = 0)
{
    Str text;
    if (!source_text(src, text)) {
        if (!err_pending())
            err_set2("TypeError", "compile() source must be a string or bytes", type_name(src));
        return Value();
    }
    Ast ast;
    ast.lex.keep_indent = (flags & PYCF_DONT_IMPLY_DEDENT) != 0;
    ast.lex.interactive = mode == CompileMode::Single;
    if (!ast.parse(text, is_str(src))) {
        if (ast.lex.wants_more && (flags & PYCF_ALLOW_INCOMPLETE_INPUT)) {
            Buf<160> m;
            m.put(err_message());
            err_set("_IncompleteInputError", m.str());
            return err_set_file(filename, text), Value();
        }
        if (codec && !ast.lex.codec.empty() && !codec->assign(ast.lex.codec.str()))
            return oom(), Value();
        return err_set_file(filename, text), Value();
    }
    // Nothing to evaluate yet: at a prompt the expression has not been typed.
    if (mode == CompileMode::Eval && (flags & PYCF_ALLOW_INCOMPLETE_INPUT)) {
        bool any = false;
        for (const Token &t : ast.lex.tokens)
            if (t.kind != Tok::Newline && t.kind != Tok::Indent && t.kind != Tok::Dedent &&
                t.kind != Tok::End)
                any = true;
        if (!any)
            ast.lex.wants_more = true;
    }
    // The blocks are still open, so what parsed is not the whole command.
    if (ast.lex.wants_more && (flags & PYCF_ALLOW_INCOMPLETE_INPUT)) {
        err_set("_IncompleteInputError", "incomplete input");
        return err_set_file(filename, text), Value();
    }
    if (tree) {
        Value made = ast_tree(ast, mode == CompileMode::Single, mode == CompileMode::Eval);
        if (made.is_nil())
            err_set_file(filename, text);
        return made;
    }
    Value code = py_compile(ast, filename, mode);
    if (code.is_nil())
        err_set_file(filename, text);
    return code;
}

R b_compile(const CallArgs &a, Value &out)
{
    if (a.nkw) {
        // The keywords into their places, so what is parked and redone is
        // positional.
        static const Str NAMES[] = {
            "source",   "filename",         "mode",  "flags", "dont_inherit",
            "optimize", "_feature_version", "module"
        };
        Value v[8];
        if (!fn_take(a, "compile", NAMES, 3, v))
            return R::Err;
        if (!v[7].is_nil() && !is_none(v[7]) && !is_str(v[7])) {
            Buf<96> b;
            b.put("compile() argument 'module' must be str or None, not ").put(type_name(v[7]));
            return err_set("TypeError", b.str());
        }
        Value pos[6] = { v[0],
                         v[1],
                         v[2],
                         v[3].is_nil() ? Value::of_int(0) : v[3],
                         v[4].is_nil() ? value_bool(false) : v[4],
                         v[5].is_nil() ? Value::of_int(-1) : v[5] };
        Roots pin{ pos, 6 };
        CallArgs b;
        b.args  = pos;
        b.nargs = 6;
        return b_compile(b, out);
    }
    if (!args_only(a, "compile", 3, 6))
        return R::Err;
    if (!is_str(a.args[1]))
        return err_set2("TypeError", "compile() filename must be a string", type_name(a.args[1]));
    CompileMode mode = CompileMode::Exec;
    if (!compile_mode(a.args[2], mode))
        return R::Err;
    i64 flags = 0;
    if (a.nargs > 3 && !is_none(a.args[3]) && !int_to_i64(a.args[3], flags))
        return err_set("TypeError", "compile() flags must be an int");
    i64 optimize = -1;
    if (a.nargs > 5 && !int_to_i64(a.args[5], optimize)) {
        Buf<96> b;
        b.put("'").put(type_name(a.args[5])).put("' object cannot be interpreted as an integer");
        return err_set("TypeError", b.str());
    }
    if (optimize < -1 || optimize > 2)
        return err_set("ValueError", "compile(): invalid optimize value");
    Root rf{ a.args[1] };
    String codec;
    compile_level_for(i32(optimize));
    out = compile_source(a.args[0], str_of(rf.v)->str(), mode, &codec, (flags & PYCF_ONLY_AST) != 0,
                         flags);
    compile_level_for(-1);
    if (out.is_nil() && !codec.empty())
        return park_decode(a, codec.str(), b_compile, out);
    return out.is_nil() ? R::Err : R::Ok;
}

FrameObj *caller_frame()
{
    Value f = vm_frame();
    return f.is_nil() ? nullptr : frame_of(f);
}

// A module or a class body keeps a real namespace, and that is its locals. A
// function's locals are frame slots, so what comes back there is a snapshot.
Value frame_locals(FrameObj *f)
{
    if (!f->locals.is_nil())
        return f->locals;
    CodeObj *c = code_of(f->code);
    DictObj *d = dict_new();
    if (!d)
        return oom(), Value();
    Root rd{ obj_value(d) };
    for (usize i = 0; i < c->varnames.size(); i++)
        if (!f->slots()[i].is_nil() &&
            dict_set(dict_at(rd.v), c->varnames[i], f->slots()[i]) != R::Ok)
            return Value();
    if (f->cells.is_nil())
        return rd.v;
    TupleObj *t = static_cast<TupleObj *>(f->cells.obj());
    usize own   = c->cellvars.size();
    for (usize i = 0; i < t->len; i++) {
        Value cell = t->items()[i];
        Value name = i < own ? c->cellvars[i] : c->freevars[i - own];
        if (cell.is_nil() || static_cast<CellObj *>(cell.obj())->v.is_nil())
            continue;
        if (dict_set(dict_at(rd.v), name, static_cast<CellObj *>(cell.obj())->v) != R::Ok)
            return Value();
    }
    if (!f->extra.is_nil()) {
        usize at = 0;
        Value k, x;
        while (table_next(dict_at(f->extra)->t, at, k, x))
            if (dict_set(dict_at(rd.v), k, x) != R::Ok)
                return Value();
    }
    return rd.v;
}

R b_globals(const CallArgs &a, Value &out)
{
    if (!args_only(a, "globals", 0, 0))
        return R::Err;
    FrameObj *f = caller_frame();
    if (!f)
        return err_set("SystemError", "globals() outside a frame");
    out = f->globals;
    return R::Ok;
}

R b_locals(const CallArgs &a, Value &out)
{
    if (!args_only(a, "locals", 0, 0))
        return R::Err;
    FrameObj *f = caller_frame();
    if (!f)
        return err_set("SystemError", "locals() outside a frame");
    out = frame_locals(f);
    return out.is_nil() ? R::Err : R::Ok;
}

R b_vars(const CallArgs &a, Value &out)
{
    if (!args_only(a, "vars", 0, 1))
        return R::Err;
    if (!a.nargs)
        return b_locals(a, out);
    StrObj *d = str_intern("__dict__");
    if (!d)
        return oom();
    if (py_getattr(a.args[0], d, out) != R::Ok) {
        err_clear();
        return err_set2("TypeError", "vars() argument must have __dict__", type_name(a.args[0]));
    }
    return R::Ok;
}

// exec and eval both run a code object, so both park. Only the loop may push
// the frame that runs it. s[0] is the function made over the code, and j says
// whether the answer is wanted.
R run_step(ContObj *k, Value in)
{
    if (k->i++ == 0)
        return cont_call(k, k->s[0], Value(), 0);
    return cont_done(k, k->j ? in : value_none());
}

// The globals and locals an exec runs in. CPython's rule is one line. Neither
// given means the caller's own, globals alone serves as both, and locals alone
// leaves the globals the caller's.
R take_namespaces(const CallArgs &a, Root &globals, Root &locals)
{
    Value g = a.nargs > 1 ? a.args[1] : value_none();
    Value l = a.nargs > 2 ? a.args[2] : value_none();
    if (!is_none(g) && !is_dict(g))
        return err_set2("TypeError", "globals must be a real dict", type_name(g));
    if (!is_none(l) && !is_dict(l))
        return err_set2("TypeError", "locals must be a mapping", type_name(l));

    FrameObj *f = caller_frame();
    if (!f)
        return err_set("SystemError", "exec() outside a frame");
    globals = is_none(g) ? f->globals : g;
    if (!is_none(l))
        locals = l;
    else if (!is_none(g))
        locals = g;
    else
        locals = frame_locals(f);
    if (locals.v.is_nil())
        return R::Err;
    // A namespace of one's own still reaches the builtins by name.
    return put_builtins(dict_at(globals.v)) ? R::Ok : R::Err;
}

R b_exec(const CallArgs &a, Value &out);
R b_eval(const CallArgs &a, Value &out);

R run_code(const CallArgs &a, CompileMode mode, bool want, Value &out)
{
    Root globals, locals;
    if (take_namespaces(a, globals, locals) != R::Ok)
        return R::Err;

    Root code{ a.args[0] };
    if (!is_code(code.v)) {
        Str text;
        if (!source_text(code.v, text))
            return err_pending()
                       ? R::Err
                       : err_set2("TypeError", "source must be a string, bytes or a code object",
                                  type_name(code.v));
        // An expression may be written with space in front of it; a statement
        // may not, because there the indentation means something.
        if (mode == CompileMode::Eval)
            while (text.size() && (text[0] == ' ' || text[0] == '\t'))
                text = text.substr(1);
        Ast ast;
        if (!ast.parse(text, is_str(code.v))) {
            if (!ast.lex.codec.empty())
                return park_decode(a, ast.lex.codec.str(),
                                   mode == CompileMode::Eval ? b_eval : b_exec, out);
            return err_set_file("<string>", text), R::Err;
        }
        code = py_compile(ast, "<string>", mode);
        if (code.v.is_nil())
            return err_set_file("<string>", text), R::Err;
    }

    Root fn{ func_new(code.v, globals.v) };
    if (fn.v.is_nil())
        return R::Err;
    Root kv{ cont_new(run_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = fn.v;
    k->j       = want;
    k->locals  = locals.v;
    out        = kv.v;
    return R::Ok;
}

// Both take `globals` and `locals` by name as well; annotationlib does.
R run_named(const CallArgs &a, Str who, CompileMode mode, bool want, Value &out)
{
    if (!a.nkw)
        return args_only(a, who, 1, 3) ? run_code(a, mode, want, out) : R::Err;
    static const Str NAMES[] = { "source", "globals", "locals" };
    Value v[3];
    if (!fn_take(a, who, NAMES, 3, 1, v))
        return R::Err;
    Roots pin{ v, 3 };
    u32 n = v[2].is_nil() ? (v[1].is_nil() ? 1 : 2) : 3;
    if (n == 3 && v[1].is_nil())
        v[1] = value_none();
    CallArgs b;
    b.args  = v;
    b.nargs = n;
    return run_code(b, mode, want, out);
}

R b_exec(const CallArgs &a, Value &out)
{
    return run_named(a, "exec", CompileMode::Exec, false, out);
}

R b_eval(const CallArgs &a, Value &out)
{
    return run_named(a, "eval", CompileMode::Eval, true, out);
}

// ------------------------------------------------------------------- dir()

bool dir_dict(SetObj *into, Value d)
{
    if (!is_dict(d))
        return true;
    usize at = 0;
    Value k, v;
    while (table_next(static_cast<DictObj *>(d.obj())->t, at, k, v))
        if (set_add(into, k) != R::Ok)
            return false;
    return true;
}

// A type's own names and every base's, which is what the MRO already lists.
bool dir_type(SetObj *into, Value cls)
{
    Value mro = type_obj(cls)->mro;
    if (!is_tuple(mro))
        return dir_dict(into, type_obj(cls)->dict);
    Root rc{ cls };
    TupleObj *t = static_cast<TupleObj *>(mro.obj());
    for (usize i = 0; i < t->len; i++)
        if (!dir_dict(into, type_obj(t->items()[i])->dict))
            return false;
    // What CPython keeps as descriptors and this answers without them:
    // object's __class__, and a class's __dict__ and __weakref__.
    Root into_v{ obj_value(into) };
    Str names[3] = { "__class__", "__dict__", "__weakref__" };
    usize n      = type_obj(rc.v)->heap && !type_obj(rc.v)->nodict ? 3 : 1;
    for (usize i = 0; i < n; i++) {
        StrObj *k = str_intern(names[i]);
        if (!k || set_add(set_at(into_v.v), obj_value(k)) != R::Ok)
            return false;
    }
    return true;
}

// dir() of an instance whose class writes __dir__: its answer, sorted.
R dir_step(ContObj *k, Value in)
{
    switch (k->i++) {
    case 0:
        return cont_call(k, k->s[0], Value(), 0);
    case 1: {
        Root got{ in };
        Root fn{ native_new("sorted", b_sorted) };
        if (fn.v.is_nil())
            return R::Err;
        return cont_call(k, fn.v, got.v);
    }
    default:
        return cont_done(k, in);
    }
}

R b_dir(const CallArgs &a, Value &out)
{
    if (!args_only(a, "dir", 0, 1))
        return R::Err;
    if (a.nargs && !is_type(a.args[0]) && type_has_py_special(a.args[0], "__dir__")) {
        Root m{ type_special(a.args[0], "__dir__") };
        if (m.v.is_nil())
            return R::Err;
        Root kv{ cont_new(dir_step) };
        if (kv.v.is_nil())
            return R::Err;
        cont_of(kv.v)->s[0] = m.v;
        out                 = kv.v;
        return R::Ok;
    }
    SetObj *s = set_new();
    if (!s)
        return oom();
    Root rs{ obj_value(s) };

    if (!a.nargs) {
        FrameObj *f = caller_frame();
        Root where{ f ? frame_locals(f) : Value() };
        if (where.v.is_nil())
            return R::Err;
        if (!dir_dict(set_at(rs.v), where.v))
            return R::Err;
    } else {
        Root rv{ a.args[0] };
        if (is_module(rv.v)) {
            if (!dir_dict(set_at(rs.v), obj_value(module_dict(rv.v))))
                return R::Err;
        } else if (is_type(rv.v)) {
            if (!dir_type(set_at(rs.v), rv.v))
                return R::Err;
        } else {
            if (is_inst(rv.v) && !dir_dict(set_at(rs.v), inst_of(rv.v)->dict))
                return R::Err;
            Root cls{ type_of_value(rv.v) };
            if (cls.v.is_nil() || !dir_type(set_at(rs.v), cls.v))
                return R::Err;
        }
    }

    Vec<Value> keys;
    usize at = 0;
    Value k, v;
    while (table_next(set_at(rs.v)->t, at, k, v))
        if (!keys.push(k))
            return oom();
    Vec<u32> idx;
    for (usize i = 0; i < keys.size(); i++)
        if (!idx.push(u32(i)))
            return oom();
    if (sort_idx(keys, idx, false) != R::Ok)
        return R::Err;
    ListObj *l = list_new();
    if (!l)
        return oom();
    Root rl{ obj_value(l) };
    for (usize i = 0; i < idx.size(); i++)
        if (!list_push(list_of(rl.v), keys[idx[i]]))
            return oom();
    out = rl.v;
    return R::Ok;
}

// FunctionType(code, globals, name=None, argdefs=None, closure=None,
// kwdefaults=None): a code object made callable over a namespace of one's
// own. annotationlib calls it by keyword.
R b_function(const CallArgs &a, Value &out)
{
    static const Str NAMES[] = { "code", "globals", "name", "argdefs", "closure", "kwdefaults" };
    Value v[6];
    if (!fn_take(a, "function", NAMES, 6, 2, v))
        return R::Err;
    Roots pin{ v, 6 };
    if (!is_code(v[0]))
        return err_set2("TypeError", "function() first argument must be a code object",
                        type_name(v[0]));
    if (!is_anydict(v[1]) && !type_has_special(v[1], "__getitem__"))
        return err_set2("TypeError", "function() second argument must be a dict", type_name(v[1]));
    Root fn{ func_new(v[0], v[1]) };
    if (fn.v.is_nil())
        return R::Err;
    FuncObj *f = func_of(fn.v);
    if (!v[2].is_nil() && !is_none(v[2])) {
        if (!is_str(v[2]))
            return err_set("TypeError", "function() name must be a string");
        f->name = f->qualname = v[2];
    }
    if (!v[3].is_nil() && !is_none(v[3])) {
        if (!is_tuple(v[3]))
            return err_set("TypeError", "function() defaults must be a tuple");
        f->defaults = v[3];
    }
    if (!v[4].is_nil() && !is_none(v[4])) {
        if (!is_tuple(v[4]))
            return err_set("TypeError", "function() closure must be a tuple");
        f->closure = v[4];
    }
    if (!v[5].is_nil() && !is_none(v[5])) {
        if (!is_dict(v[5]))
            return err_set("TypeError", "function() kwdefaults must be a dict");
        f->kwdefaults = v[5];
    }
    out = fn.v;
    return R::Ok;
}

// ------------------------------------------------------------------ the map

// aiter(x): x.__aiter__(), whose answer has to be an async iterator. s[0] is
// the bound method.
R aiter_step(ContObj *k, Value in)
{
    if (k->i++ == 0)
        return cont_call(k, k->s[0], Value(), 0);
    if (!is_agen(in) && !type_has_special(in, "__anext__")) {
        Buf<96> m;
        m.put("aiter() returned not an async iterator of type '").put(type_name(in)).put("'");
        return err_set("TypeError", m.str());
    }
    return cont_done(k, in);
}

R b_aiter(const CallArgs &a, Value &out)
{
    if (!args_only(a, "aiter", 1, 1))
        return R::Err;
    if (is_agen(a.args[0])) {
        out = a.args[0];
        return R::Ok;
    }
    Root m{ type_special(a.args[0], "__aiter__") };
    if (m.v.is_nil()) {
        if (err_pending())
            return R::Err;
        Buf<96> b;
        b.put("'").put(type_name(a.args[0])).put("' object is not an async iterable");
        return err_set("TypeError", b.str());
    }
    Root kv{ cont_new(aiter_step) };
    if (kv.v.is_nil())
        return R::Err;
    cont_of(kv.v)->s[0] = m.v;
    out                 = kv.v;
    return R::Ok;
}

// anext(it[, default]): it.__anext__(), wrapped so that the end of the
// iteration is the default. s[0] is the bound method, s[1] the default or Nil.
R anext1_step(ContObj *k, Value in)
{
    if (k->i++ == 0)
        return cont_call(k, k->s[0], Value(), 0);
    if (k->s[1].is_nil())
        return cont_done(k, in);
    Value w = anext_default(in, k->s[1]);
    return w.is_nil() ? R::Err : cont_done(k, w);
}

R b_anext(const CallArgs &a, Value &out)
{
    if (!args_only(a, "anext", 1, 2))
        return R::Err;
    Root it{ a.args[0] }, dflt{ a.nargs > 1 ? a.args[1] : Value() };
    if (is_agen(it.v)) {
        Root aw{ await_new(it.v, AK_ASEND, value_none()) };
        if (aw.v.is_nil())
            return R::Err;
        out = dflt.v.is_nil() ? aw.v : anext_default(aw.v, dflt.v);
        return out.is_nil() ? R::Err : R::Ok;
    }
    Root m{ type_special(it.v, "__anext__") };
    if (m.v.is_nil()) {
        if (err_pending())
            return R::Err;
        Buf<96> b;
        b.put("'").put(type_name(it.v)).put("' object is not an async iterator");
        return err_set("TypeError", b.str());
    }
    Root kv{ cont_new(anext1_step) };
    if (kv.v.is_nil())
        return R::Err;
    cont_of(kv.v)->s[0] = m.v;
    cont_of(kv.v)->s[1] = dflt.v;
    out                 = kv.v;
    return R::Ok;
}

// input(prompt=''): s[0] the prompt, s[1] stdin, s[2] stdout.
R input_step(ContObj *k, Value in)
{
    switch (k->i++) {
    case 0:
        // stderr first, as CPython flushes it; what that raises is ignored.
        {
            Value err = sys_stream("stderr");
            if (!err.is_nil() && !is_none(err)) {
                k->catching = CATCH_ANY;
                return cont_method(k, err, "flush");
            }
        }
        [[fallthrough]];
    case 1:
        k->i        = 2;
        k->catching = CATCH_NONE;
        k->caught   = Value();
        if (!k->s[0].is_nil()) {
            String text;
            if (py_str(k->s[0], text) != R::Ok)
                return R::Err;
            Value s = str_new(text.str());
            if (s.is_nil())
                return R::Err;
            return cont_method(k, k->s[2], "write", 1, s);
        }
        [[fallthrough]];
    case 2:
        k->i = 3;
        return cont_method(k, k->s[2], "flush");
    case 3:
        return cont_method(k, k->s[1], "readline");
    default: {
        if (!is_str(in)) {
            Buf<96> m;
            m.put("object.readline() returned non-string");
            return err_set("TypeError", m.str());
        }
        Str line = str_of(in)->str();
        if (line.empty())
            return err_set("EOFError", "EOF when reading a line");
        if (line[line.size() - 1] == '\n')
            line = line.substr(0, line.size() - 1);
        Value out = str_new(line);
        return out.is_nil() ? R::Err : cont_done(k, out);
    }
    }
}

R b_input(const CallArgs &a, Value &out)
{
    if (!args_only(a, "input", 0, 1))
        return R::Err;
    Root stdin_{ sys_stream("stdin") };
    if (stdin_.v.is_nil() || is_none(stdin_.v))
        return err_pending() ? R::Err : err_set("RuntimeError", "input(): lost sys.stdin");
    Root stdout_{ sys_stream("stdout") };
    if (stdout_.v.is_nil() || is_none(stdout_.v))
        return err_pending() ? R::Err : err_set("RuntimeError", "input(): lost sys.stdout");
    Root kv{ cont_new(input_step) };
    if (kv.v.is_nil())
        return R::Err;
    cont_of(kv.v)->s[0] = a.nargs ? a.args[0] : Value();
    cont_of(kv.v)->s[1] = stdin_.v;
    cont_of(kv.v)->s[2] = stdout_.v;
    out                 = kv.v;
    return R::Ok;
}

// print(..., flush=True): s[0] the write's own continuation or Nil, s[1] the
// file, or Nil for sys.stdout.
R flush_after_step(ContObj *k, Value)
{
    switch (k->i++) {
    case 0:
        if (!k->s[0].is_nil())
            return cont_await(k, k->s[0]);
        [[fallthrough]];
    case 1: {
        k->i       = 2;
        Value file = k->s[1].is_nil() || is_none(k->s[1]) ? sys_stream("stdout") : k->s[1];
        if (file.is_nil() || is_none(file))
            return err_pending() ? R::Err : cont_done(k, value_none());
        return cont_method(k, file, "flush");
    }
    default:
        return cont_done(k, value_none());
    }
}

struct Builtin {
    Str name;
    R (*fn)(const CallArgs &, Value &out);
};

constexpr Builtin TABLE[] = {
    { "print", b_print },
    { "input", b_input },
    { "open", io_open },
    { "len", b_len },
    { "abs", b_abs },
    { "repr", b_repr },
    { "min", b_min },
    { "max", b_max },
    { "sum", b_sum },
    { "all", b_all },
    { "any", b_any },
    { "ord", b_ord },
    { "chr", b_chr },
    { "iter", b_iter },
    { "next", b_next },
    { "sorted", b_sorted },
    { "enumerate", b_enumerate },
    { "getattr", b_getattr },
    { "hasattr", b_hasattr },
    { "setattr", b_setattr },
    { "delattr", b_delattr },
    { "callable", b_callable },
    { "hash", b_hash },
    { "id", b_id },
    { "divmod", b_divmod },
    { "round", b_round },
    { "pow", b_pow },
    { "reversed", b_reversed },
    { "zip", b_zip },
    { "map", b_map },
    { "filter", b_filter },
    { "__import__", b_import },
    { "__lazy_import__", b_lazy_import },
    { "format", b_format },
    { "ascii", b_ascii },
    { "hex", b_hex },
    { "oct", b_oct },
    { "bin", b_bin },
    { "breakpoint", sys_breakpoint },
    { "compile", b_compile },
    { "eval", b_eval },
    { "exec", b_exec },
    { "globals", b_globals },
    { "locals", b_locals },
    { "vars", b_vars },
    { "dir", b_dir },
    { "aiter", b_aiter },
    { "anext", b_anext },
};

// Calling one of these is calling its type: `list(x)` is `list.__new__(x)`,
// and the name in the builtins namespace is the type object.
struct Ctor {
    const Type *type;
    R (*fn)(const CallArgs &, Value &out);
};

constexpr Ctor CTORS[] = {
    { &str_type, b_str },
    { &bool_type, b_bool },
    { &int_type, b_int },
    { &float_type, b_float },
    { &list_type, b_list },
    { &tuple_type, b_tuple },
    { &dict_type, b_dict },
    { &set_type, b_set },
    { &range_type, b_range },
    { &bytes_type, b_bytes },
    { &bytearray_type, b_bytearray },
    { &frozenset_type, b_frozenset },
    { &frozendict_type, b_frozendict },
    { &sentinel_type, b_sentinel },
    { &module_type, b_module },
    { &method_type, b_method },
    { &genalias_type, b_genericalias },
    { &complex_type, b_complex },
    { &memview_type, b_memoryview },
    { &slice_type, b_slice },
    { &func_type, b_function },
};

} // namespace

R dict_fill_keys(Value d, Value src, Value &out)
{
    Root rd{ d }, rs{ src };
    Root keys{ type_special(rs.v, "keys") };
    Root get{ type_special(rs.v, "__getitem__") };
    if (keys.v.is_nil() || get.v.is_nil())
        return err_pending() ? R::Err : not_iterable(rs.v);
    TupleObj *n = tuple_new(0);
    if (!n)
        return oom();
    Root none{ obj_value(n) };
    Root kv{ cont_new(dict_keys_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = rd.v;
    k->s[1]    = keys.v;
    k->s[2]    = get.v;
    k->s[4]    = none.v;
    k->s[5]    = none.v;
    k->s[6]    = value_none();
    out        = kv.v;
    return R::Ok;
}

R py_dict_of(const CallArgs &a, Value &out)
{
    return b_dict(a, out);
}

R py_dir(const CallArgs &a, Value &out)
{
    return b_dir(a, out);
}

bool py_callable(Value v)
{
    bool yes =
        is_func(v) || is_native(v) || is_method(v) || is_type(v) || is_exc_type(v) || is_newwrap(v);
    if (!yes && v.is_obj() && v.obj()->type == &staticmethod_type)
        return py_callable(static_cast<WrapObj *>(v.obj())->fn);
    if (!yes && is_inst(v))
        yes = !type_special(v, "__call__").is_nil();
    // A built-in whose type has a __call__ method: partial, the cache wrapper.
    if (!yes && v.is_obj() && !is_inst(v)) {
        StrObj *n = str_intern("__call__");
        Value m;
        yes = n && method_find(v, n, m) == R::Ok;
        if (!yes)
            err_clear();
    }
    return yes;
}

DictObj *builtins_dict()
{
    Home *h = here();
    if (!h)
        return oom(), nullptr;
    if (!h->builtins.is_nil())
        return static_cast<DictObj *>(h->builtins.obj());

    gc_root_hook(home_mark);
    DictObj *d = dict_new();
    if (!d)
        return oom(), nullptr;
    h->builtins = obj_value(d);

    Root modname{ str_new("builtins") };
    if (modname.v.is_nil())
        return nullptr;
    for (const Builtin &e : TABLE) {
        Root fn{ native_new(e.name, e.fn) };
        if (fn.v.is_nil())
            return nullptr;
        fn.v.obj()->flags |= OBJ_PLAINFN;
        static_cast<NativeObj *>(fn.v.obj())->owner = modname.v;
        StrObj *name                                = str_intern(e.name);
        if (!name)
            return oom(), nullptr;
        if (dict_set(static_cast<DictObj *>(h->builtins.obj()), obj_value(name), fn.v) != R::Ok)
            return nullptr;
    }
    struct Singleton {
        Str name;
        Value (*of)();
    };
    constexpr Singleton SINGLETONS[] = { { "None", value_none },
                                         { "Ellipsis", value_ellipsis },
                                         { "NotImplemented", value_notimpl } };
    for (const Singleton &g : SINGLETONS) {
        StrObj *n = str_intern(g.name);
        if (!n ||
            dict_set(static_cast<DictObj *>(h->builtins.obj()), obj_value(n), g.of()) != R::Ok)
            return nullptr;
    }
    StrObj *dbg = str_intern("__debug__");
    if (!dbg || dict_set(static_cast<DictObj *>(h->builtins.obj()), obj_value(dbg),
                         value_bool(compile_optimize() == 0)) != R::Ok)
        return nullptr;

    // The namespace is the builtins module's, and says so.
    StrObj *nk = str_intern("__name__");
    Root nv{ str_new("builtins") };
    if (!nk || nv.v.is_nil() ||
        dict_set(static_cast<DictObj *>(h->builtins.obj()), obj_value(nk), nv.v) != R::Ok)
        return nullptr;

    if (!type_install(static_cast<DictObj *>(h->builtins.obj())))
        return nullptr;
    for (const Ctor &c : CTORS) {
        Root fn{ native_new(c.type->name, c.fn) };
        if (fn.v.is_nil() || !type_set_ctor(c.type, fn.v))
            return nullptr;
    }
    if (!methods_install())
        return nullptr;
    return static_cast<DictObj *>(h->builtins.obj());
}

Value builtins_module()
{
    Home *h = here();
    if (!h)
        return oom(), Value();
    if (h->builtins_mod.is_nil()) {
        DictObj *b = builtins_dict();
        Root m{ module_new("builtins") };
        if (!b || m.v.is_nil() || !module_defaults(b))
            return Value();
        static_cast<ModuleObj *>(m.v.obj())->dict = obj_value(b);
        h->builtins_mod                           = m.v;
    }
    return h->builtins_mod;
}

Value format_special(Value v, Str spec)
{
    if (!is_inst(v))
        return Value();
    Root m{ type_special(v, "__format__") };
    if (m.v.is_nil())
        return Value();
    Root sv{ str_new(spec) };
    if (sv.v.is_nil())
        return Value();
    Value kv = cont_new(format_step);
    if (kv.is_nil())
        return Value();
    cont_of(kv)->s[0] = m.v;
    cont_of(kv)->a[0] = sv.v;
    return kv;
}

R format_field(Value v, Str spec, u32 conv, i32 min_digits, Value &out)
{
    Root rv{ v };
    u32 want = SHOW_STR;
    if (conv == CONV_REPR)
        want = SHOW_REPR;
    else if (conv == CONV_ASCII)
        want = SHOW_ASCII;
    else if (conv != CONV_STR) {
        // No conversion at all: a __format__ of its own decides everything.
        out = format_special(rv.v, spec);
        if (!out.is_nil())
            return R::Ok;
        if (err_pending())
            return R::Err;
        // What is left is object.__format__: the built-in conversion, or
        // str(self) when the spec is empty -- and that may be Python, or a
        // container holding something whose __repr__ is.
        if (spec.size() || min_digits >= 0) {
            String text;
            if (format_builtin(rv.v, spec, text, min_digits) != R::Ok)
                return R::Err;
            out = str_new(text.str());
            return out.is_nil() ? R::Err : R::Ok;
        }
    }

    if (show(rv.v, want, out) != R::Ok)
        return R::Err;
    if (!spec.size())
        return R::Ok;
    // What show() gave back is held across an allocation from here on, so it
    // is pinned: a collection in str_new would otherwise sweep it.
    Root got{ out };
    // The conversion made text; the spec applies to that, which is str's own
    // and needs no further call.
    if (is_cont(got.v)) {
        Value sv = str_new(spec);
        if (sv.is_nil())
            return R::Err;
        cont_of(got.v)->s[4] = sv;
        out                  = got.v;
        return R::Ok;
    }
    String text;
    if (format_builtin(got.v, spec, text) != R::Ok)
        return R::Err;
    out = str_new(text.str());
    return out.is_nil() ? R::Err : R::Ok;
}

Value show_special(Value v, bool want_str)
{
    Root m{ text_of(v, want_str) };
    if (m.v.is_nil())
        return Value();
    Value kv = cont_new(one_step);
    if (kv.is_nil())
        return Value();
    cont_of(kv)->s[0] = m.v;
    cont_of(kv)->j    = WANT_STR;
    return kv;
}

void print_sink(String *out)
{
    Home *h = here();
    if (h)
        h->sink = out;
}

bool put_builtins(DictObj *into)
{
    Root rd{ obj_value(into) };
    StrObj *key = str_intern("__builtins__");
    if (!key)
        return oom() == R::Ok;
    Value had;
    R r = dict_get(dict_at(rd.v), obj_value(key), had);
    if (r != R::NotImpl)
        return r == R::Ok;
    Root m{ builtin_module("builtins") };
    if (m.v.is_nil())
        return false;
    return dict_set(dict_at(rd.v), obj_value(key), m.v) == R::Ok;
}

// stdout's rule, which sysmod's std_put states.
bool display_put(String *sink, Str text)
{
    if (!has_surrogate(text))
        return sink->append(text) || oom() == R::Ok;
    String b;
    return std_encode(text, true, b) && (sink->append(b.str()) || oom() == R::Ok);
}

// `builtins._`, which the default displayhook leaves the value in.
bool set_underscore(Value v)
{
    Root rv{ v };
    Root m{ builtin_module("builtins") };
    StrObj *k = str_intern("_");
    if (m.v.is_nil() || !k)
        return false;
    return dict_set(module_dict(m.v), obj_value(k), rv.v) == R::Ok;
}

// Whether sys.stdout is still the one the process started with, which is
// written straight into the VM's buffer.
bool display_native()
{
    Value now = sys_stream("stdout");
    return now.is_nil() || now == sys_stream("__stdout__");
}

// The repr and a newline written to a sys.stdout of the program's own, as
// two calls. `out` is the continuation making them, or Nil.
R display_elsewhere(Str repr, Value &out)
{
    String text;
    if (!text.append(repr) || !text.push('\n'))
        return oom();
    usize cuts[] = { 0, repr.size(), text.size() };
    return sys_write(Value(), text.str(), out, Span<const usize>(cuts, 3));
}

// The repr has come back. It goes where print's output goes. s[0] is the
// value, so `_` is set once the line is out, as CPython's hook does.
R display_step(ContObj *k, Value in)
{
    if (k->i == 1)
        return cont_done(k, value_none());
    Home *h = here();
    if (!h || !h->sink)
        return err_set("SystemError", "nothing to print to");
    String text;
    if (py_str(in, text) != R::Ok)
        return err_pending() ? R::Err : oom();
    if (!display_native()) {
        if (!set_underscore(k->s[0]))
            return R::Err;
        Root w;
        if (display_elsewhere(text.str(), w.v) != R::Ok)
            return R::Err;
        if (w.v.is_nil())
            return cont_done(k, value_none());
        k->i = 1;
        return cont_await(k, w.v);
    }
    if (!text.push('\n'))
        return oom();
    if (!display_put(h->sink, text.str()))
        return R::Err;
    if (!set_underscore(k->s[0]))
        return R::Err;
    return cont_done(k, value_none());
}

// sys.displayhook's default, which is also what PrintExpr does when nothing
// has replaced it: print the repr unless the value is None, and leave the
// value in `builtins._`.
R py_display_value(Value v, Value &out)
{
    out     = Value();
    Home *h = here();
    if (is_none(v) || !h || !h->sink)
        return R::Ok;
    Root rv{ v }, text;
    if (!set_underscore(value_none()))
        return R::Err;
    if (show(rv.v, SHOW_REPR, text.v) != R::Ok)
        return R::Err;
    // The repr is Python's own, so the writing waits on it; the chain
    // answers None, which is what the hook returns.
    if (is_cont(text.v)) {
        Value kv = cont_new(display_step);
        if (kv.is_nil())
            return R::Err;
        cont_of(kv)->s[0]     = rv.v;
        cont_of(text.v)->next = kv;
        cont_of(kv)->drop     = true;
        out                   = text.v;
        return R::Ok;
    }
    if (!display_native()) {
        if (!set_underscore(rv.v))
            return R::Err;
        return display_elsewhere(str_of(text.v)->str(), out);
    }
    if (!display_put(h->sink, str_of(text.v)->str()) || !display_put(h->sink, "\n"))
        return R::Err;
    return set_underscore(rv.v) ? R::Ok : R::Err;
}

R py_display(Value v, Value &out)
{
    out = Value();
    Root rv{ v };
    // A hook of the program's own is Python, so it is handed back as a call.
    Root hook{ sys_stream("displayhook") };
    if (!hook.v.is_nil() && !sys_is_default_displayhook(hook.v)) {
        Root args{ obj_value(tuple_new(1)) };
        if (args.v.is_nil())
            return oom();
        static_cast<TupleObj *>(args.v.obj())->items()[0] = rv.v;
        out                                               = attr_invoke(hook.v, args.v);
        return out.is_nil() ? R::Err : R::Ok;
    }
    return py_display_value(rv.v, out);
}
