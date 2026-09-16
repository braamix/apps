// The builtins. One that must call back into Python -- `sorted(key=)`,
// `min(key=)` -- parks in a ContObj and lets the VM drive it, which is ground
// rule 2; see call.h. `map` and `filter` still wait, because they call back
// from inside the iterator protocol and py_next has no way to suspend.
#include "builtin.h"

#include "abc.h"
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
#include "import.h"
#include "intern.h"
#include "iter.h"
#include "kernel/alloc.h"
#include "kernel/fmt.h"
#include "kernel/text.h"
#include "math/ftoa.h"
#include "math/math.h"
#include "method.h"
#include "ops.h"
#include "parse.h"
#include "type.h"
#include "vm.h"
#include "weak.h"

namespace {

// All of these outlive the process, so they are roots rather than heap that
// the collector may take: a static Value is not traced, so they live in one
// dict that is.
struct Home {
    Value builtins;
    Value builtins_mod;
    Value sys;
    Value weakref;
    Value abc;
    Value argv;
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
    gc_mark(home->weakref);
    gc_mark(home->abc);
    gc_mark(home->sys);
    gc_mark(home->argv);
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
    if (!is_inst(v))
        return Value();
    Root found;
    StrObj *n = str_intern(want_str ? "__str__" : "__repr__");
    if (n && type_lookup(inst_of(v)->cls, n, found.v) == R::Ok)
        return method_new(found.v, v);
    if (want_str) {
        n = str_intern("__repr__");
        if (n && type_lookup(inst_of(v)->cls, n, found.v) == R::Ok)
            return method_new(found.v, v);
    }
    return Value();
}

R print_line(const Value *args, u32 n, Str sep, Str end);

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
    } else if (is_dict(v) || is_set(v)) {
        const Table &t =
            is_dict(v) ? static_cast<DictObj *>(v.obj())->t : static_cast<SetObj *>(v.obj())->t;
        usize at = 0;
        Value k, x;
        while (table_next(t, at, k, x)) {
            if (!collect_one(k, into, depth + 1))
                return false;
            if (is_dict(v) && !collect_one(x, into, depth + 1))
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
    if (is_dict(rv.v) || is_set(rv.v)) {
        bool dict = is_dict(rv.v);
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
    if (print_line(xs->items.data(), u32(xs->items.size()), sep, end) != R::Ok)
        return R::Err;
    return cont_done(k, value_none());
}

R b_print(const CallArgs &a, Value &out)
{
    Str sep = " ", end = "\n";
    for (u32 k = 0; k < a.nkw; k++) {
        Str name = is_str(a.kwnames[k]) ? str_of(a.kwnames[k])->str() : Str();
        if (!is_str(a.kwvals[k]))
            return err_set2("TypeError", "print() argument must be str", name);
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
        out                 = kv.v;
        return R::Ok;
    }
    if (print_line(a.args, a.nargs, sep, end) != R::Ok)
        return R::Err;
    out = value_none();
    return R::Ok;
}

R print_line(const Value *args, u32 n, Str sep, Str end)
{
    String line;
    for (u32 i = 0; i < n; i++) {
        if (i && !line.append(sep))
            return oom();
        if (py_str(args[i], line) != R::Ok)
            return R::Err;
    }
    if (!line.append(end))
        return oom();
    String *sink = here() ? here()->sink : nullptr;
    return sink && !sink->append(line.str()) ? oom() : R::Ok;
}

// ------------------------------------------------------------- conversions

// A builtin whose work on a class instance is one special method. `j` says
// what the answer has to be.
// WANT_FOUND is hasattr's: True because the call returned at all, whatever
// it returned. WANT_BOOL is __bool__'s, which is the value's own truth.
enum : u32 { WANT_ANY, WANT_INT, WANT_STR, WANT_BOOL, WANT_FOUND, WANT_HASH };

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
        if (!as_index(in, n))
            return err_set2("TypeError", "a special method returned a non-integer", type_name(in));
        break;
    case WANT_HASH:
        // A __hash__ of any width is truncated to one, as CPython does.
        if (!is_intval(in))
            return err_set2("TypeError", "__hash__ returned a non-integer", type_name(in));
        in = Value::of_int(i32(int_hash_of(in)) & 0x3fffffff);
        break;
    case WANT_STR:
        if (!is_str(in))
            return err_set2("TypeError", "a special method returned a non-string", type_name(in));
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
    return radix_show(a, "hex", 16, "0x", out);
}

R b_oct(const CallArgs &a, Value &out)
{
    return radix_show(a, "oct", 8, "0o", out);
}

R b_bin(const CallArgs &a, Value &out)
{
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

// The encoding argument str(), bytes() and bytearray() take. UTF-8 and ASCII
// only; phase 22 brings the codecs.
R encoding_ok(const CallArgs &a, u32 at)
{
    for (u32 i = at; i < a.nargs; i++) {
        if (!is_str(a.args[i]))
            return err_set2("TypeError", "the encoding must be a str", type_name(a.args[i]));
        if (i > at) // the errors argument: always strict here
            continue;
        Str e = str_of(a.args[i])->str();
        if (!(e == "utf-8" || e == "utf8" || e == "UTF-8" || e == "UTF8" || e == "ascii" ||
              e == "ASCII"))
            return err_set2("LookupError", "unknown encoding", e);
    }
    return R::Ok;
}

R b_str(const CallArgs &a, Value &out)
{
    if (!args_only(a, "str", 0, 3))
        return R::Err;
    if (!a.nargs) {
        out = str_new("");
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (a.nargs > 1) {
        Str octets;
        if (!bytes_like(a.args[0], octets))
            return err_set2("TypeError", "decoding to str: a bytes-like object is required",
                            type_name(a.args[0]));
        if (encoding_ok(a, 1) != R::Ok)
            return R::Err;
        out = str_new(octets);
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (is_str(a.args[0])) {
        out = a.args[0];
        return R::Ok;
    }
    return show(a.args[0], SHOW_STR, out);
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
    return R::Ok;
}

// int(s, base): space, a sign, an optional 0x/0o/0b prefix, then digits with
// `_` allowed between them, over any width. bigint.cpp owns the grammar.
R int_of_text(Str s, i64 base, Value &out)
{
    if (base != 0 && (base < 2 || base > 36))
        return err_set("ValueError", "int() base must be >= 2 and <= 36, or 0");
    out = int_parse(s, u32(base));
    return out.is_nil() ? R::Err : R::Ok;
}

R b_int(const CallArgs &a, Value &out)
{
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
        return int_of_text(text, base, out);
    }
    out = one_special(a.args[0], "__int__", WANT_INT);
    if (!out.is_nil())
        return R::Ok;
    if (err_pending())
        return R::Err;
    if (is_intval(a.args[0])) {
        // int(True) is 1, so bool does not simply pass through.
        out = is_bool(a.args[0]) ? Value::of_int(is_true(a.args[0]) ? 1 : 0) : a.args[0];
        return R::Ok;
    }
    if (is_float(a.args[0])) {
        f64 x = float_of(a.args[0]);
        if (isnan(x))
            return err_set("ValueError", "cannot convert float NaN to integer");
        if (isinf(x))
            return err_set("OverflowError", "cannot convert float infinity to integer");
        // Toward zero, which is what Python asks for.
        out = int_from_f64(x);
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (textual)
        return int_of_text(text, 10, out);
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
    if (cp < 0 || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
        return err_set("ValueError", "chr() arg not in range(0x110000)");

    char b[4];
    usize n = 0;
    u32 c   = u32(cp);
    if (c < 0x80) {
        b[n++] = char(c);
    } else if (c < 0x800) {
        b[n++] = char(0xc0 | (c >> 6));
        b[n++] = char(0x80 | (c & 0x3f));
    } else if (c < 0x10000) {
        b[n++] = char(0xe0 | (c >> 12));
        b[n++] = char(0x80 | ((c >> 6) & 0x3f));
        b[n++] = char(0x80 | (c & 0x3f));
    } else {
        b[n++] = char(0xf0 | (c >> 18));
        b[n++] = char(0x80 | ((c >> 12) & 0x3f));
        b[n++] = char(0x80 | ((c >> 6) & 0x3f));
        b[n++] = char(0x80 | (c & 0x3f));
    }
    out = obj_value(str_raw(Str(b, n)));
    return out.is_nil() ? oom() : R::Ok;
}

R b_bytes(const CallArgs &a, Value &out)
{
    if (!args_only(a, "bytes", 0, 3))
        return R::Err;
    if (a.nargs == 1 && parks(a, 0))
        return iter_park(a, 0, b_bytes, out);
    if (a.nargs > 1) {
        if (!is_str(a.args[0]))
            return err_set2("TypeError", "encoding without a string argument",
                            type_name(a.args[0]));
        if (encoding_ok(a, 1) != R::Ok)
            return R::Err;
        out = bytes_new(str_of(a.args[0])->str());
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (!a.nargs || is_bytes(a.args[0])) {
        out = a.nargs ? a.args[0] : bytes_new(Str());
        return out.is_nil() ? R::Err : R::Ok;
    }
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
    if (!a.nargs) {
        out = complex_new(0, 0);
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (is_str(a.args[0])) {
        if (a.nargs > 1)
            return err_set("TypeError", "complex() can't take second arg if first is a string");
        f64 re = 0, im = 0;
        if (!complex_parse(str_of(a.args[0])->str(), re, im))
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
    out = complex_new(re, im);
    return out.is_nil() ? R::Err : R::Ok;
}

R b_float(const CallArgs &a, Value &out)
{
    if (!args_only(a, "float", 0, 1))
        return R::Err;
    f64 v = 0;
    if (a.nargs && !as_number(a.args[0], v))
        return err_set2("TypeError", "float() argument must be a number", type_name(a.args[0]));
    out = float_new(v);
    return out.is_nil() ? R::Err : R::Ok;
}

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

R b_dict(const CallArgs &a, Value &out)
{
    if (a.nargs > 1)
        return err_set("TypeError", "dict() takes at most one positional argument");
    if (parks(a, 0))
        return iter_park(a, 0, b_dict, out);
    DictObj *d = dict_new();
    if (!d)
        return oom();
    Root rd{ obj_value(d) };
    if (a.nargs) {
        Root src{ a.args[0] };
        if (is_dict(src.v)) {
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
            for (;;) {
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
                if (n != 2)
                    return err_set("ValueError",
                                   "dictionary update sequence element "
                                   "has the wrong length");
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
    i64 n[3] = { 0, 0, 1 };
    for (u32 i = 0; i < a.nargs; i++)
        if (!as_index(a.args[i], n[i]))
            return err_set2("TypeError", "range() argument must be an integer",
                            type_name(a.args[i]));
    if (a.nargs == 1) {
        n[1] = n[0];
        n[0] = 0;
    }
    out = range_new(n[0], n[1], n[2]);
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
    for (;;) {
        Root got;
        R r = py_next(it.v, got.v);
        if (r == R::Err)
            return R::Err;
        if (r == R::NotImpl)
            break;
        Value next;
        if (py_binop(acc.v, got.v, Op::Add, next) != R::Ok)
            return R::Err;
        acc = next;
    }
    out = acc.v;
    return R::Ok;
}

R every(const CallArgs &a, bool want, Value &out)
{
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
        if (py_truth(got.v) == want) {
            out = value_bool(want);
            return R::Ok;
        }
    }
    out = value_bool(!want);
    return R::Ok;
}

R b_all(const CallArgs &a, Value &out)
{
    if (!args_only(a, "all", 1, 1))
        return R::Err;
    if (parks(a, 0))
        return iter_park(a, 0, b_all, out);
    return every(a, false, out);
}

R b_any(const CallArgs &a, Value &out)
{
    if (!args_only(a, "any", 1, 1))
        return R::Err;
    if (parks(a, 0))
        return iter_park(a, 0, b_any, out);
    return every(a, true, out);
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
    if (!args_only(a, "iter", 1, 1))
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
    if (is_gen(a.args[0]) || type_has_special(a.args[0], "__next__")) {
        Root m{ is_gen(a.args[0]) ? genrun_new(a.args[0], GR_NEXT)
                                  : type_special(a.args[0], "__next__") };
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
    Buf<96> m;
    m.put("'").put(type_name(a.args[0])).put("' object has no attribute '");
    m.put(str_of(a.args[1])->str()).put("'");
    return err_set("AttributeError", m.str());
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
    Value v  = a.args[0];
    bool yes = is_func(v) || is_native(v) || is_method(v) || is_type(v) || is_exc_type(v);
    if (!yes && is_inst(v))
        yes = !type_special(v, "__call__").is_nil();
    out = value_bool(yes);
    return R::Ok;
}

R b_hash(const CallArgs &a, Value &out)
{
    if (!args_only(a, "hash", 1, 1))
        return R::Err;
    if (type_unhashable(a.args[0]))
        return err_set2("TypeError", "unhashable type", type_name(a.args[0]));
    out = one_special(a.args[0], "__hash__", WANT_HASH);
    if (!out.is_nil())
        return R::Ok;
    if (err_pending())
        return R::Err;
    u32 h = 0;
    if (py_hash(a.args[0], h) != R::Ok)
        return R::Err;
    out = Value::of_int(i32(h) & 0x3fffffff);
    return R::Ok;
}

R b_id(const CallArgs &a, Value &out)
{
    if (!args_only(a, "id", 1, 1))
        return R::Err;
    out = int_from_i64(a.args[0].is_obj() ? i64(usize(a.args[0].obj())) : i64(a.args[0].w));
    return out.is_nil() ? R::Err : R::Ok;
}

// --------------------------------------------------- arithmetic with a shape

R b_divmod(const CallArgs &a, Value &out)
{
    if (!args_only(a, "divmod", 2, 2))
        return R::Err;
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
    if (!as_number(a.args[0], v))
        return err_set2("TypeError", "a number is required", type_name(a.args[0]));
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

R b_pow(const CallArgs &a, Value &out)
{
    if (!args_only(a, "pow", 2, 3))
        return R::Err;
    if (a.nargs < 3 || is_none(a.args[2]))
        return py_binop(a.args[0], a.args[1], Op::Pow, out);

    if (!is_intval(a.args[0]) || !is_intval(a.args[1]) || !is_intval(a.args[2]))
        return err_set("TypeError", "pow() 3rd argument not allowed unless all arguments are ints");
    if (int_is_neg(a.args[1]))
        return err_set("ValueError", "base is not invertible for the given modulus");
    return int_power(a.args[0], a.args[1], a.args[2], out);
}

// ----------------------------------------------------------- the iterators

R b_reversed(const CallArgs &a, Value &out)
{
    if (!args_only(a, "reversed", 1, 1))
        return R::Err;
    out = reversed_new(a.args[0]);
    return out.is_nil() ? R::Err : R::Ok;
}

R b_zip(const CallArgs &a, Value &out)
{
    if (a.nkw)
        return err_set("TypeError", "zip() takes no keyword arguments");
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
    out = zip_new(rt.v);
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

R b_bytearray(const CallArgs &a, Value &out)
{
    if (!args_only(a, "bytearray", 0, 3))
        return R::Err;
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

R b_exit(const CallArgs &a, Value &out)
{
    if (!args_only(a, "exit", 0, 1))
        return R::Err;
    TupleObj *args = tuple_new(a.nargs);
    if (!args)
        return oom();
    for (u32 i = 0; i < a.nargs; i++)
        args->items()[i] = a.args[i];
    Value e = exc_new(exc_find("SystemExit"), obj_value(args));
    if (e.is_nil())
        return R::Err;
    out = Value();
    return err_set_value(e);
}

// ------------------------------------------------- compile, eval and exec

// What a source may arrive as. CPython takes bytes here too.
bool source_text(Value v, Str &out)
{
    if (is_str(v)) {
        out = str_of(v)->str();
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

// Parse and compile. The Ast is a stack object, so the code object it leaves
// behind is what outlives this.
Value compile_source(Value src, Str filename, CompileMode mode)
{
    Str text;
    if (!source_text(src, text))
        return err_set2("TypeError", "compile() source must be a string or bytes", type_name(src)),
               Value();
    Ast ast;
    if (!ast.parse(text))
        return Value();
    return py_compile(ast, filename, mode);
}

R b_compile(const CallArgs &a, Value &out)
{
    if (!args_only(a, "compile", 3, 6))
        return R::Err;
    if (!is_str(a.args[1]))
        return err_set2("TypeError", "compile() filename must be a string", type_name(a.args[1]));
    CompileMode mode = CompileMode::Exec;
    if (!compile_mode(a.args[2], mode))
        return R::Err;
    Root rf{ a.args[1] };
    out = compile_source(a.args[0], str_of(rf.v)->str(), mode);
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

R run_code(const CallArgs &a, CompileMode mode, bool want, Value &out)
{
    Root globals, locals;
    if (take_namespaces(a, globals, locals) != R::Ok)
        return R::Err;

    Root code{ a.args[0] };
    if (!is_code(code.v)) {
        Str text;
        if (!source_text(code.v, text))
            return err_set2("TypeError", "source must be a string, bytes or a code object",
                            type_name(code.v));
        // An expression may be written with space in front of it; a statement
        // may not, because there the indentation means something.
        if (mode == CompileMode::Eval)
            while (text.size() && (text[0] == ' ' || text[0] == '\t'))
                text = text.substr(1);
        Ast ast;
        if (!ast.parse(text))
            return R::Err;
        code = py_compile(ast, "<string>", mode);
        if (code.v.is_nil())
            return R::Err;
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

R b_exec(const CallArgs &a, Value &out)
{
    if (!args_only(a, "exec", 1, 3))
        return R::Err;
    return run_code(a, CompileMode::Exec, false, out);
}

R b_eval(const CallArgs &a, Value &out)
{
    if (!args_only(a, "eval", 1, 3))
        return R::Err;
    return run_code(a, CompileMode::Eval, true, out);
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
    TupleObj *t = static_cast<TupleObj *>(mro.obj());
    for (usize i = 0; i < t->len; i++)
        if (!dir_dict(into, type_obj(t->items()[i])->dict))
            return false;
    return true;
}

R b_dir(const CallArgs &a, Value &out)
{
    if (!args_only(a, "dir", 0, 1))
        return R::Err;
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

// FunctionType(code, globals, name=None, argdefs=None, closure=None), which
// is how a code object becomes callable over a namespace of one's own.
R b_function(const CallArgs &a, Value &out)
{
    if (!args_only(a, "function", 2, 5))
        return R::Err;
    if (!is_code(a.args[0]))
        return err_set2("TypeError", "function() first argument must be a code object",
                        type_name(a.args[0]));
    if (!is_dict(a.args[1]))
        return err_set2("TypeError", "function() second argument must be a dict",
                        type_name(a.args[1]));
    Root fn{ func_new(a.args[0], a.args[1]) };
    if (fn.v.is_nil())
        return R::Err;
    FuncObj *f = func_of(fn.v);
    if (a.nargs > 2 && !is_none(a.args[2])) {
        if (!is_str(a.args[2]))
            return err_set("TypeError", "function() name must be a string");
        f->name = f->qualname = a.args[2];
    }
    if (a.nargs > 3 && !is_none(a.args[3])) {
        if (!is_tuple(a.args[3]))
            return err_set("TypeError", "function() defaults must be a tuple");
        f->defaults = a.args[3];
    }
    if (a.nargs > 4 && !is_none(a.args[4])) {
        if (!is_tuple(a.args[4]))
            return err_set("TypeError", "function() closure must be a tuple");
        f->closure = a.args[4];
    }
    out = fn.v;
    return R::Ok;
}

// ------------------------------------------------------------------ the map

struct Builtin {
    Str name;
    R (*fn)(const CallArgs &, Value &out);
};

constexpr Builtin TABLE[] = {
    { "print", b_print },     { "len", b_len },           { "abs", b_abs },
    { "repr", b_repr },       { "min", b_min },           { "max", b_max },
    { "sum", b_sum },         { "all", b_all },           { "any", b_any },
    { "ord", b_ord },         { "chr", b_chr },           { "iter", b_iter },
    { "next", b_next },       { "sorted", b_sorted },     { "enumerate", b_enumerate },
    { "getattr", b_getattr }, { "hasattr", b_hasattr },   { "setattr", b_setattr },
    { "delattr", b_delattr }, { "callable", b_callable }, { "hash", b_hash },
    { "id", b_id },           { "divmod", b_divmod },     { "round", b_round },
    { "pow", b_pow },         { "reversed", b_reversed }, { "zip", b_zip },
    { "map", b_map },         { "filter", b_filter },     { "__import__", b_import },
    { "format", b_format },   { "ascii", b_ascii },       { "hex", b_hex },
    { "oct", b_oct },         { "bin", b_bin },           { "compile", b_compile },
    { "eval", b_eval },       { "exec", b_exec },         { "globals", b_globals },
    { "locals", b_locals },   { "vars", b_vars },         { "dir", b_dir },
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
    { &complex_type, b_complex },
    { &memview_type, b_memoryview },
    { &slice_type, b_slice },
    { &func_type, b_function },
};

} // namespace

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

    for (const Builtin &e : TABLE) {
        Root fn{ native_new(e.name, e.fn) };
        if (fn.v.is_nil())
            return nullptr;
        StrObj *name = str_intern(e.name);
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

Value builtin_module(Str name)
{
    Home *h = here();
    if (!h)
        return oom(), Value();
    // `builtins` is the namespace every frame already falls back to, wrapped
    // in a module so it can be imported like anything else.
    if (name == "builtins") {
        if (h->builtins_mod.is_nil()) {
            DictObj *b = builtins_dict();
            Root m{ module_new("builtins") };
            if (!b || m.v.is_nil())
                return Value();
            static_cast<ModuleObj *>(m.v.obj())->dict = obj_value(b);
            h->builtins_mod                           = m.v;
        }
        return h->builtins_mod;
    }
    // The weak references, and the two counters that go with them. This is
    // the floor CPython's weakref.py stands on, not that module itself.
    if (name == "_weakref") {
        if (h->weakref.is_nil()) {
            Root m{ module_new("_weakref") };
            if (m.v.is_nil() || !weak_install(module_dict(m.v)))
                return Value();
            h->weakref = m.v;
        }
        return h->weakref;
    }
    // The abstract base classes, which abc.py prefers over its own fallback.
    if (name == "_abc") {
        if (h->abc.is_nil()) {
            Root m{ module_new("_abc") };
            StrObj *n = str_intern("issubclass");
            Value fn;
            if (m.v.is_nil() || !n)
                return Value();
            if (dict_get(builtins_dict(), obj_value(n), fn) != R::Ok ||
                !abc_install(module_dict(m.v), fn))
                return Value();
            h->abc = m.v;
        }
        return h->abc;
    }
    // Nil and no error: the loader goes looking for a file instead.
    if (name != "sys")
        return Value();
    if (!h->sys.is_nil())
        return h->sys;

    Value m = module_new("sys");
    if (m.is_nil())
        return Value();
    h->sys       = m;
    StrObj *argv = str_intern("argv");
    StrObj *exit = str_intern("exit");
    Value fn     = native_new("exit", b_exit);
    if (!argv || !exit || fn.is_nil())
        return Value();
    Root rf{ fn };
    if (dict_set(module_dict(h->sys), obj_value(argv), h->argv.is_nil() ? value_none() : h->argv) !=
            R::Ok ||
        dict_set(module_dict(h->sys), obj_value(exit), rf.v) != R::Ok)
        return Value();

    // sys.implementation, which a portable test reads to know where it is.
    Root impl{ module_new("implementation") };
    if (impl.v.is_nil())
        return Value();
    StrObj *nm = str_intern("name");
    Value who  = str_new("braam");
    if (!nm || who.is_nil() || dict_set(module_dict(impl.v), obj_value(nm), who) != R::Ok)
        return Value();
    // sys.flags, which a test reads to know whether docstrings are there.
    // The rest of sys is phase 18; these three are what this suite asks for.
    Root flags{ module_new("flags") };
    if (flags.v.is_nil())
        return Value();
    static constexpr Str FLAG_NAMES[] = { "optimize", "debug", "verbose" };
    for (Str one : FLAG_NAMES) {
        StrObj *f = str_intern(one);
        if (!f || dict_set(module_dict(flags.v), obj_value(f), Value::of_int(0)) != R::Ok)
            return Value();
    }
    StrObj *fl = str_intern("flags");
    if (!fl || dict_set(module_dict(h->sys), obj_value(fl), flags.v) != R::Ok)
        return Value();

    StrObj *key = str_intern("implementation");
    StrObj *pl  = str_intern("platform");
    Value plat  = str_new("braam");
    if (!key || !pl || plat.is_nil() ||
        dict_set(module_dict(h->sys), obj_value(key), impl.v) != R::Ok ||
        dict_set(module_dict(h->sys), obj_value(pl), plat) != R::Ok)
        return Value();

    // The language this aims at, and the implementation's own number. The rest
    // of sys is phase 18.
    Root ver{ str_new("3.9.0 (braam)") };
    TupleObj *vi = tuple_new(5);
    if (ver.v.is_nil() || !vi)
        return Value();
    Root rvi{ obj_value(vi) };
    Value parts[5] = { Value::of_int(3), Value::of_int(9), Value::of_int(0), Value(),
                       Value::of_int(0) };
    parts[3]       = str_new("final");
    if (parts[3].is_nil())
        return Value();
    for (u32 i = 0; i < 5; i++)
        static_cast<TupleObj *>(rvi.v.obj())->items()[i] = parts[i];
    StrObj *vn = str_intern("version");
    StrObj *vt = str_intern("version_info");
    if (!vn || !vt || dict_set(module_dict(h->sys), obj_value(vn), ver.v) != R::Ok ||
        dict_set(module_dict(h->sys), obj_value(vt), rvi.v) != R::Ok)
        return Value();

    // The cache and the search path are the loader's, and a program reads and
    // writes both through here.
    StrObj *mods = str_intern("modules");
    StrObj *path = str_intern("path");
    DictObj *sm  = sys_modules();
    Root sp{ sys_path() };
    if (!mods || !path || !sm || sp.v.is_nil() ||
        dict_set(module_dict(h->sys), obj_value(mods), obj_value(sm)) != R::Ok ||
        dict_set(module_dict(h->sys), obj_value(path), sp.v) != R::Ok)
        return Value();
    return h->sys;
}

void sys_set_argv(Value argv)
{
    Home *h = here();
    if (h)
        h->argv = argv;
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

// The repr has come back. It goes where print's output goes.
R display_step(ContObj *k, Value in)
{
    Home *h = here();
    if (!h || !h->sink)
        return err_set("SystemError", "nothing to print to");
    String text;
    if (py_str(in, text) != R::Ok)
        return R::Err;
    if (!h->sink->append(text.str()) || !h->sink->push('\n'))
        return oom();
    return cont_done(k, value_none());
}

R py_display(Value v, Value &out)
{
    out     = Value();
    Home *h = here();
    if (is_none(v) || !h || !h->sink)
        return R::Ok;
    Root rv{ v }, text;
    if (show(rv.v, SHOW_REPR, text.v) != R::Ok)
        return R::Err;
    // The repr is Python's own, so the writing waits on it.
    if (is_cont(text.v)) {
        Value kv = cont_new(display_step);
        if (kv.is_nil())
            return R::Err;
        cont_of(text.v)->next = kv;
        cont_of(kv)->drop     = true;
        out                   = text.v;
        return R::Ok;
    }
    if (!h->sink->append(str_of(text.v)->str()) || !h->sink->push('\n'))
        return oom();
    return R::Ok;
}
