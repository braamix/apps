// The builtins. One that must call back into Python -- `sorted(key=)`,
// `min(key=)` -- parks in a ContObj and lets the VM drive it, which is ground
// rule 2; see call.h. `map` and `filter` still wait, because they call back
// from inside the iterator protocol and py_next has no way to suspend.
#include "builtin.h"

#include "call.h"
#include "exc.h"
#include "gc.h"
#include "intern.h"
#include "iter.h"
#include "kernel/alloc.h"
#include "kernel/fmt.h"
#include "ops.h"

namespace {

// All of these outlive the process, so they are roots rather than heap that
// the collector may take: a static Value is not traced, so they live in one
// dict that is.
struct Home {
    Value builtins;
    Value sys;
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
    gc_mark(home->sys);
    gc_mark(home->argv);
}

R oom()
{
    return err_set("MemoryError", "out of memory");
}

// ------------------------------------------------------------------- print

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

    String line;
    for (u32 i = 0; i < a.nargs; i++) {
        if (i && !line.append(sep))
            return oom();
        if (py_str(a.args[i], line) != R::Ok)
            return R::Err;
    }
    if (!line.append(end))
        return oom();

    String *sink = here() ? here()->sink : nullptr;
    if (sink && !sink->append(line.str()))
        return oom();
    out = value_none();
    return R::Ok;
}

// ------------------------------------------------------------- conversions

R b_len(const CallArgs &a, Value &out)
{
    if (!args_only(a, "len", 1, 1))
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
    i64 n = 0;
    if (as_index(a.args[0], n)) {
        out = int_from_i64(n < 0 ? -n : n);
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (is_float(a.args[0])) {
        f64 v = float_of(a.args[0]);
        out   = float_new(v < 0 ? -v : v);
        return out.is_nil() ? R::Err : R::Ok;
    }
    return err_set2("TypeError", "bad operand type for abs()", type_name(a.args[0]));
}

R b_repr(const CallArgs &a, Value &out)
{
    if (!args_only(a, "repr", 1, 1))
        return R::Err;
    String s;
    if (py_repr(a.args[0], s) != R::Ok)
        return R::Err;
    out = str_new(s.str());
    return out.is_nil() ? R::Err : R::Ok;
}

R b_str(const CallArgs &a, Value &out)
{
    if (!args_only(a, "str", 0, 1))
        return R::Err;
    if (!a.nargs) {
        out = str_new("");
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (is_str(a.args[0])) {
        out = a.args[0];
        return R::Ok;
    }
    String s;
    if (py_str(a.args[0], s) != R::Ok)
        return R::Err;
    out = str_new(s.str());
    return out.is_nil() ? R::Err : R::Ok;
}

R b_bool(const CallArgs &a, Value &out)
{
    if (!args_only(a, "bool", 0, 1))
        return R::Err;
    out = value_bool(a.nargs && py_truth(a.args[0]));
    return R::Ok;
}

R b_int(const CallArgs &a, Value &out)
{
    if (!args_only(a, "int", 0, 1))
        return R::Err;
    if (!a.nargs) {
        out = Value::of_int(0);
        return R::Ok;
    }
    i64 n = 0;
    if (as_index(a.args[0], n)) {
        out = int_from_i64(n);
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (is_float(a.args[0])) {
        // Toward zero, which is what C++ does and what Python asks for.
        out = int_from_i64(i64(float_of(a.args[0])));
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (is_str(a.args[0])) {
        Str s   = str_of(a.args[0])->str();
        usize i = 0;
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
            i++;
        bool neg = false;
        if (i < s.size() && (s[i] == '-' || s[i] == '+'))
            neg = s[i++] == '-';
        i64 v    = 0;
        usize at = i;
        for (; i < s.size() && s[i] >= '0' && s[i] <= '9'; i++)
            v = v * 10 + (s[i] - '0');
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
            i++;
        if (at == i || i != s.size())
            return err_set2("ValueError", "invalid literal for int() with base 10", s);
        out = int_from_i64(neg ? -v : v);
        return out.is_nil() ? R::Err : R::Ok;
    }
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
    if (a.nargs)
        return err_set("TypeError", "dict() takes no positional arguments yet");
    DictObj *d = dict_new();
    if (!d)
        return oom();
    Root rd{ obj_value(d) };
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

// s[0] values, s[1] the key function, s[2] their keys, s[3] whether to reverse.
R sort_finish(ContObj *k)
{
    ListObj *vals = list_of(k->s[0]);
    ListObj *keys = list_of(k->s[2]);
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
    Root key, dflt;
    bool rev = false, has = false;
    if (take_kw(a, "sorted", KW_KEY | KW_REVERSE, key.v, rev, dflt.v, has) != R::Ok)
        return R::Err;

    ListObj *l = py_list_of(a.args[0]);
    if (!l)
        return R::Err;
    Root rl{ obj_value(l) };

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
    if (k->i > 0) {
        Value item  = xs->items[k->i - 1];
        bool better = k->s[2].is_nil();
        if (!better && py_cmp(in, k->s[3], k->j ? Cmp::Lt : Cmp::Gt, better) != R::Ok)
            return R::Err;
        if (better) {
            k->s[2] = item;
            k->s[3] = in;
        }
    }
    if (k->i < xs->items.size())
        return cont_call(k, k->s[1], xs->items[k->i++]);
    return cont_done(k, k->s[2]);
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
    ContObj *k = cont_of(kv.v);
    k->s[0]    = rl.v;
    k->s[1]    = key.v;
    k->j       = least ? 1 : 0;
    out        = kv.v;
    return R::Ok;
}

R b_min(const CallArgs &a, Value &out)
{
    return fold(a, true, "min", out);
}

R b_max(const CallArgs &a, Value &out)
{
    return fold(a, false, "max", out);
}

R b_sum(const CallArgs &a, Value &out)
{
    if (!args_only(a, "sum", 1, 2))
        return R::Err;
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
    return every(a, false, out);
}

R b_any(const CallArgs &a, Value &out)
{
    if (!args_only(a, "any", 1, 1))
        return R::Err;
    return every(a, true, out);
}

R b_enumerate(const CallArgs &a, Value &out)
{
    Root seq, start{ Value::of_int(0) };
    if (a.nargs > 2)
        return err_set("TypeError", "enumerate() takes from 1 to 2 arguments");
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

R b_next(const CallArgs &a, Value &out)
{
    if (!args_only(a, "next", 1, 2))
        return R::Err;
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

// ------------------------------------------------------------------ the map

struct Builtin {
    Str name;
    R (*fn)(const CallArgs &, Value &out);
};

constexpr Builtin TABLE[] = {
    { "print", b_print }, { "len", b_len },       { "abs", b_abs },
    { "repr", b_repr },   { "str", b_str },       { "bool", b_bool },
    { "int", b_int },     { "float", b_float },   { "list", b_list },
    { "tuple", b_tuple }, { "dict", b_dict },     { "set", b_set },
    { "range", b_range }, { "min", b_min },       { "max", b_max },
    { "sum", b_sum },     { "all", b_all },       { "any", b_any },
    { "ord", b_ord },     { "chr", b_chr },       { "iter", b_iter },
    { "next", b_next },   { "sorted", b_sorted }, { "enumerate", b_enumerate },
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
        Value fn = native_new(e.name, e.fn);
        if (fn.is_nil())
            return nullptr;
        StrObj *name = str_intern(e.name);
        if (!name)
            return oom(), nullptr;
        if (dict_set(static_cast<DictObj *>(h->builtins.obj()), obj_value(name), fn) != R::Ok)
            return nullptr;
    }
    StrObj *none = str_intern("None");
    if (!none ||
        dict_set(static_cast<DictObj *>(h->builtins.obj()), obj_value(none), value_none()) != R::Ok)
        return nullptr;
    return static_cast<DictObj *>(h->builtins.obj());
}

Value builtin_module(Str name)
{
    Home *h = here();
    if (!h)
        return oom(), Value();
    if (name != "sys")
        return err_set2("ImportError", "no module named", name), Value();
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
    return h->sys;
}

void sys_set_argv(Value argv)
{
    Home *h = here();
    if (h)
        h->argv = argv;
}

void print_sink(String *out)
{
    Home *h = here();
    if (h)
        h->sink = out;
}
