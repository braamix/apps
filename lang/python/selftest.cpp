// `python --selftest`: the object heap, the collector and the core types,
// checked from inside. There is no Python to run yet, so these are the
// assertions instead, and test/pygc.mjs reads what they print.
#include "selftest.h"

#include "bigint.h"
#include "code.h"
#include "compile.h"
#include "exc.h"
#include "gc.h"
#include "intern.h"
#include "kernel/fmt.h"
#include "ops.h"
#include "parse.h"

namespace {

// A reason with values in it. Valid until the next call, which is why every
// caller returns it straight away.
Buf<160> detail;

Str why(Str what, i64 got, i64 want)
{
    detail.clear();
    detail.put(what).put(": got ");
    if (got < 0) {
        detail.put('-');
        got = -got;
    }
    detail.put(u64(got)).put(", want ");
    if (want < 0) {
        detail.put('-');
        want = -want;
    }
    detail.put(u64(want));
    return detail.str();
}

Str why_s(Str what, Str got, Str want)
{
    detail.clear();
    detail.put(what).put(": got <").put(got).put(">, want <").put(want).put('>');
    return detail.str();
}

usize live()
{
    return gc_stats().objects;
}

Value str_v(Str s)
{
    return str_new(s);
}

Str check_repr(Value v, Str want)
{
    String buf;
    if (py_repr(v, buf) != R::Ok)
        return why_s("repr failed", err_message(), want);
    return buf.str() == want ? Str() : why_s("repr", buf.str(), want);
}

// ------------------------------------------------------------------ phase 1

Str t_value()
{
    if (!Value().is_nil() || Value().is_obj() || Value().is_int())
        return "Nil is not nil";
    constexpr i32 NUMBERS[] = { 0, 1, -1, 12345, Value::SMALL_MIN, Value::SMALL_MAX };
    for (i32 n : NUMBERS) {
        Value v = Value::of_int(n);
        if (!v.is_int() || v.is_obj() || v.is_nil())
            return "a small int is not tagged as one";
        if (v.as_int() != n)
            return why("small int round trip", v.as_int(), n);
    }
    if (Value::fits_small(i64(Value::SMALL_MAX) + 1) || !Value::fits_small(Value::SMALL_MIN))
        return "the small range is wrong";

    Value none = value_none();
    if (!none.is_obj() || none.is_int() || type_of(none) != &none_type)
        return "None is not an object of NoneType";
    if (value_bool(true) == value_bool(false))
        return "True and False are one object";
    if (type_of(Value::of_int(1)) != &int_type)
        return "a small int has no type";
    return Str();
}

Str t_strings()
{
    Root a{ str_v("hello") }, b{ str_v("hello") };
    if (a.v.is_nil() || b.v.is_nil())
        return "str_new returned nothing";
    StrObj *x = str_of(a.v), *y = str_of(b.v);
    if (x->str() != "hello")
        return "the bytes did not survive the copy";
    if (x == y)
        return "str_new returned the same object twice";
    if (x->hash != y->hash)
        return "equal bytes hash differently";
    if (str_of(str_v(""))->len != 0)
        return "the empty string is not empty";
    if (!str_new("\xff\xfe").is_nil() || err_kind() != "ValueError")
        return "bad UTF-8 was accepted";
    err_clear();
    return Str();
}

Str t_intern()
{
    usize before  = intern_count();
    StrObj *first = str_intern("spam");
    if (!first || first != str_intern("spam"))
        return "the same bytes gave two objects";
    if (str_intern("eggs") == first)
        return "different bytes gave one object";
    if (intern_count() != before + 2)
        return why("interned", i64(intern_count() - before), 2);

    gc_collect();
    if (str_intern("spam") != first || first->str() != "spam")
        return "an interned string did not survive a collection";
    return Str();
}

Str t_collect()
{
    usize base = live();
    for (usize i = 0; i < 100; i++)
        if (str_v("garbage").is_nil())
            return "allocation failed";
    if (live() != base + 100)
        return why("allocated", i64(live() - base), 100);
    gc_collect();
    if (live() != base)
        return why("left after the sweep", i64(live() - base), 0);
    return Str();
}

Str t_root()
{
    usize base = live();
    {
        Root r{ str_v("kept") };
        for (usize i = 0; i < 50; i++)
            str_v("dropped");
        gc_collect();
        if (live() != base + 1)
            return why("survivors", i64(live() - base), 1);
        if (str_of(r.v)->str() != "kept")
            return "the pinned string was not itself afterwards";
    }
    gc_collect();
    if (live() != base)
        return why("left once the pin went out of scope", i64(live() - base), 0);
    return Str();
}

Str t_tuple()
{
    usize base = live();
    Root t{ obj_value(tuple_new(3)) };
    if (t.v.is_nil())
        return "tuple_new returned nothing";
    TupleObj *tup = static_cast<TupleObj *>(t.v.obj());
    for (usize i = 0; i < 3; i++)
        if (tup->items()[i] != Value())
            return "a fresh tuple is not all Nil";

    tup->items()[0] = str_v("a");
    tup->items()[1] = Value::of_int(7);
    tup->items()[2] = obj_value(tuple_new(0));
    gc_collect();
    if (live() != base + 3)
        return why("the tuple and what it holds", i64(live() - base), 3);
    if (str_of(tup->items()[0])->str() != "a")
        return "the traced string was not itself afterwards";
    return Str();
}

Str t_list()
{
    usize base = live();
    Root l{ obj_value(list_new()) };
    for (usize i = 0; i < 1000; i++) {
        Value s = str_v("item");
        if (s.is_nil() || !list_push(list_of(l.v), s))
            return "the list would not grow";
    }
    gc_collect();
    if (live() != base + 1001)
        return why("the list and its items", i64(live() - base), 1001);
    if (list_of(l.v)->items.size() != 1000)
        return why("items", i64(list_of(l.v)->items.size()), 1000);
    if (str_of(list_of(l.v)->items[999])->str() != "item")
        return "the last item was not itself afterwards";
    return Str();
}

Str t_cycle()
{
    usize base = live();
    {
        Root ra{ obj_value(list_new()) };
        ListObj *b = list_new();
        list_push(list_of(ra.v), obj_value(b));
        list_push(b, ra.v);             // a -> b -> a
        list_push(list_of(ra.v), ra.v); // and a -> a
        gc_collect();
        if (live() != base + 2)
            return why("the cycle while pinned", i64(live() - base), 2);
    }
    gc_collect();
    if (live() != base)
        return why("a cycle nothing points at", i64(live() - base), 0);
    return Str();
}

Str t_deep()
{
    usize base = live();
    {
        Root head{ obj_value(list_new()) };
        ListObj *at = list_of(head.v);
        for (usize i = 0; i < 10000; i++) {
            ListObj *next = list_new();
            if (!next || !list_push(at, obj_value(next)))
                return "the chain would not grow";
            at = next;
        }
        // The marker threads its worklist through the objects, so ten thousand
        // deep costs no native stack. Recursion here would trap instead.
        gc_collect();
        if (live() != base + 10001)
            return why("the chain", i64(live() - base), 10001);
    }
    gc_collect();
    if (live() != base)
        return why("left after the chain went", i64(live() - base), 0);
    return Str();
}

Str t_stress()
{
    usize base  = live();
    usize swept = gc_stats().collections;
    gc_stress(true);
    Root l{ obj_value(list_new()) };
    for (usize i = 0; i < 200; i++) {
        // Every one of these allocations collects first. Anything reachable
        // only from a C++ local would be gone by the next line.
        Root rt{ obj_value(tuple_new(2)) };
        TupleObj *t   = static_cast<TupleObj *>(rt.v.obj());
        t->items()[0] = obj_value(str_intern("pinned"));
        t->items()[1] = str_v("fresh");
        if (!list_push(list_of(l.v), rt.v))
            return "the list would not grow";
    }
    gc_stress(false);
    if (gc_stats().collections <= swept + 200)
        return "stress did not collect at every allocation";
    // The list, 200 tuples, 200 fresh strings, and "pinned" interned once.
    if (live() != base + 402)
        return why("what the stressed loop built", i64(live() - base), 402);
    for (usize i = 0; i < 200; i++) {
        TupleObj *t = static_cast<TupleObj *>(list_of(l.v)->items[i].obj());
        if (str_of(t->items()[0])->str() != "pinned" || str_of(t->items()[1])->str() != "fresh")
            return "an item did not survive the stress intact";
    }
    return Str();
}

Str t_threshold()
{
    usize before = gc_stats().collections;
    for (usize i = 0; i < 20000; i++)
        if (str_v("0123456789012345678901234567890123456789").is_nil())
            return "allocation failed";
    if (gc_stats().collections == before)
        return "the threshold never fired";
    if (gc_stats().freed == 0)
        return "nothing was swept";
    return Str();
}

// ------------------------------------------------------------------ phase 2

Str binop_is(i64 a, i64 b, Op op, i64 want)
{
    Value out;
    if (py_binop(Value::of_int(i32(a)), Value::of_int(i32(b)), op, out) != R::Ok)
        return why_s("binop failed", err_message(), "no error");
    i64 got = 0;
    if (!as_index(out, got))
        return why_s("binop gave a non-int", type_name(out), "int");
    return got == want ? Str() : why("binop", got, want);
}

Str t_numbers()
{
    struct Case {
        i64 a, b;
        Op op;
        i64 want;
    };
    // Floor division and modulo take the sign of the divisor, as in CPython.
    constexpr Case CASES[] = {
        { 2, 3, Op::Add, 5 },        { 2, 3, Op::Sub, -1 },       { 7, 6, Op::Mul, 42 },
        { 1, -3, Op::FloorDiv, -1 }, { 1, -3, Op::Mod, -2 },      { -7, 2, Op::FloorDiv, -4 },
        { -7, 2, Op::Mod, 1 },       { 7, -2, Op::FloorDiv, -4 }, { 7, -2, Op::Mod, -1 },
        { 2, 10, Op::Pow, 1024 },    { 6, 3, Op::And, 2 },        { 6, 3, Op::Or, 7 },
        { 6, 3, Op::Xor, 5 },        { 1, 10, Op::Lsh, 1024 },    { -8, 2, Op::Rsh, -2 },
    };
    for (const Case &c : CASES) {
        Str bad = binop_is(c.a, c.b, c.op, c.want);
        if (!bad.empty())
            return bad;
    }

    Value out;
    if (py_binop(Value::of_int(1), Value::of_int(0), Op::Div, out) != R::Err ||
        err_kind() != "ZeroDivisionError")
        return "1/0 did not raise ZeroDivisionError";
    err_clear();
    if (py_binop(Value::of_int(1), Value::of_int(0), Op::Mod, out) != R::Err)
        return "1%0 did not raise";
    err_clear();

    // What does not fit the value word promotes rather than wrapping, and
    // the two shapes still compare and hash as one number.
    if (py_binop(Value::of_int(Value::SMALL_MAX), Value::of_int(2), Op::Mul, out) != R::Ok ||
        !is_big(out))
        return "a product past the value word did not become a big";
    {
        Root wide{ out };
        Value half;
        if (py_binop(wide.v, Value::of_int(2), Op::FloorDiv, half) != R::Ok ||
            half != Value::of_int(Value::SMALL_MAX))
            return "a big divided back down did not become small again";
        u32 h1 = 0, h2 = 0;
        Value same;
        if (py_binop(wide.v, Value::of_int(0), Op::Add, same) != R::Ok || !is_big(same))
            return "adding zero to a big did not keep it";
        if (py_hash(wide.v, h1) != R::Ok || py_hash(same, h2) != R::Ok || h1 != h2)
            return "two equal bigs hashed differently";
        bool eq = false;
        if (py_eq(wide.v, same, eq) != R::Ok || !eq)
            return "two equal bigs did not compare equal";
    }

    if (py_binop(Value::of_int(2), Value::of_int(-1), Op::Pow, out) != R::Ok || !is_float(out) ||
        float_of(out) != 0.5)
        return "2 ** -1 is not 0.5";
    if (py_binop(Value::of_int(7), Value::of_int(2), Op::Div, out) != R::Ok || !is_float(out) ||
        float_of(out) != 3.5)
        return "7 / 2 is not 3.5";
    return Str();
}

Str t_floats()
{
    struct Case {
        f64 v;
        Str want;
    };
    // Every one of these is what CPython's repr() prints.
    const Case CASES[] = {
        { 1.0, "1.0" },
        { 0.1, "0.1" },
        { -0.0, "-0.0" },
        { 1e16, "1e+16" },
        { 1e17, "1e+17" },
        { 1e-5, "1e-05" },
        { 1.0 / 3.0, "0.3333333333333333" },
        { 2.5, "2.5" },
        { 100.0, "100.0" },
        { 1e22, "1e+22" },
        { 1.5e-8, "1.5e-08" },
        { 123456789.0, "123456789.0" },
        { 3.14, "3.14" },
        { -2.75, "-2.75" },
        { 1e100, "1e+100" },
        { 0.1 + 0.2, "0.30000000000000004" },
        { 0.0, "0.0" },
        { 1e15, "1000000000000000.0" },
        { 1e-4, "0.0001" },
        { 1e-10, "1e-10" },
        { 5e-324, "5e-324" },
        { 1e-323, "1e-323" },
        { 2.2250738585072014e-308, "2.2250738585072014e-308" },
        { 9007199254740992.0, "9007199254740992.0" },
        { -1e16, "-1e+16" },
        { 1234567890123456.0, "1234567890123456.0" },
    };
    for (const Case &c : CASES) {
        Str bad = check_repr(float_new(c.v), c.want);
        if (!bad.empty())
            return bad;
    }

    f64 zero = 0;
    Str bad  = check_repr(float_new(1 / zero), "inf");
    if (!bad.empty())
        return bad;
    bad = check_repr(float_new(-1 / zero), "-inf");
    if (!bad.empty())
        return bad;
    bad = check_repr(float_new(zero / zero), "nan");
    if (!bad.empty())
        return bad;

    // An integral float must hash as the equal int, or {1: 'a'}[1.0] misses.
    u32 h1 = 0, hf = 0;
    if (py_hash(Value::of_int(1), h1) != R::Ok || py_hash(float_new(1.0), hf) != R::Ok)
        return "hashing a number failed";
    if (h1 != hf)
        return why("hash(1.0)", hf, h1);
    return Str();
}

Str t_compare()
{
    struct Case {
        Value a, b;
        Cmp op;
        bool want;
    };
    Root s1{ str_v("abc") }, s2{ str_v("abd") };
    const Case CASES[] = {
        { Value::of_int(1), Value::of_int(2), Cmp::Lt, true },
        { Value::of_int(2), Value::of_int(2), Cmp::Le, true },
        { Value::of_int(2), float_new(2.0), Cmp::Eq, true },
        { Value::of_int(2), float_new(2.5), Cmp::Lt, true },
        { value_bool(true), Value::of_int(1), Cmp::Eq, true },
        { s1.v, s2.v, Cmp::Lt, true },
        { s1.v, s1.v, Cmp::Eq, true },
        { s1.v, Value::of_int(1), Cmp::Eq, false },
        { value_none(), value_none(), Cmp::Eq, true },
    };
    for (const Case &c : CASES) {
        bool got = !c.want;
        if (py_cmp(c.a, c.b, c.op, got) != R::Ok)
            return why_s("compare failed", err_message(), "no error");
        if (got != c.want)
            return why_s("compare", got ? "true" : "false", c.want ? "true" : "false");
    }

    bool ignored = false;
    if (py_cmp(s1.v, Value::of_int(1), Cmp::Lt, ignored) != R::Err || err_kind() != "TypeError")
        return "'abc' < 1 did not raise TypeError";
    err_clear();
    return Str();
}

Str t_strtext()
{
    Root s{ str_v("caf\xc3\xa9!") }; // café!
    usize n = 0;
    if (py_len(s.v, n) != R::Ok || n != 5)
        return why("len('cafe!')", i64(n), 5);
    if (str_of(s.v)->len != 6)
        return why("bytes of 'cafe!'", i64(str_of(s.v)->len), 6);
    if (str_char_at(str_of(s.v), 3) != 0xe9)
        return why("the fourth character", i64(str_char_at(str_of(s.v), 3)), 0xe9);

    Value ch;
    if (py_getitem(s.v, Value::of_int(3), ch) != R::Ok)
        return "indexing failed";
    Str bad = check_repr(ch, "'\xc3\xa9'");
    if (!bad.empty())
        return bad;
    if (py_getitem(s.v, Value::of_int(-1), ch) != R::Ok || str_of(ch)->str() != "!")
        return "a negative index did not count from the end";
    if (py_getitem(s.v, Value::of_int(5), ch) != R::Err || err_kind() != "IndexError")
        return "an index past the end did not raise IndexError";
    err_clear();

    Value joined;
    Root a{ str_v("ab") }, b{ str_v("cd") };
    if (py_binop(a.v, b.v, Op::Add, joined) != R::Ok || str_of(joined)->str() != "abcd")
        return "'ab' + 'cd' is not 'abcd'";
    if (py_binop(a.v, Value::of_int(3), Op::Mul, joined) != R::Ok ||
        str_of(joined)->str() != "ababab")
        return "'ab' * 3 is not 'ababab'";
    bool has = false;
    if (py_contains(s.v, b.v, has) != R::Ok || has)
        return "'cd' in 'cafe!' is not false";
    if (py_contains(s.v, a.v, has) != R::Ok || has)
        return "'ab' in 'cafe!' is not false";
    Root f{ str_v("af") };
    if (py_contains(s.v, f.v, has) != R::Ok || !has)
        return "'af' in 'cafe!' is not true";
    return Str();
}

Str t_reprs()
{
    struct Case {
        Value v;
        Str want;
    };

    Root s{ str_v("a\"b") }, s2{ str_v("it's") }, s3{ str_v("a\\b\nc\td") };
    Root s4{ str_v("caf\xc3\xa9") }, s5{ str_v("\x01\x1f\x7f") };
    Root by{ bytes_new(Str("ab\x00\xff\n", 5)) };
    Root t0{ obj_value(tuple_new(0)) }, t1{ obj_value(tuple_new(1)) };
    Root t2{ obj_value(tuple_new(2)) };
    static_cast<TupleObj *>(t1.v.obj())->items()[0] = Value::of_int(1);
    static_cast<TupleObj *>(t2.v.obj())->items()[0] = Value::of_int(1);
    static_cast<TupleObj *>(t2.v.obj())->items()[1] = Value::of_int(2);
    Root inner{ obj_value(list_new()) };
    list_push(list_of(inner.v), Value::of_int(2));
    Root outer{ obj_value(list_new()) };
    list_push(list_of(outer.v), Value::of_int(1));
    list_push(list_of(outer.v), inner.v);

    const Case CASES[] = {
        { value_none(), "None" },
        { value_bool(true), "True" },
        { value_bool(false), "False" },
        { Value::of_int(0), "0" },
        { Value::of_int(-42), "-42" },
        { s.v, "'a\"b'" },
        { s2.v, "\"it's\"" },
        { s3.v, "'a\\\\b\\nc\\td'" },
        { s4.v, "'caf\xc3\xa9'" },
        { s5.v, "'\\x01\\x1f\\x7f'" },
        { by.v, "b'ab\\x00\\xff\\n'" },
        { t0.v, "()" },
        { t1.v, "(1,)" },
        { t2.v, "(1, 2)" },
        { outer.v, "[1, [2]]" },
    };
    for (const Case &c : CASES) {
        Str bad = check_repr(c.v, c.want);
        if (!bad.empty())
            return bad;
    }

    // A list holding itself prints the way CPython prints one.
    Root self{ obj_value(list_new()) };
    list_push(list_of(self.v), self.v);
    return check_repr(self.v, "[[...]]");
}

Str t_dict()
{
    Root d{ obj_value(dict_new()) };
    DictObj *dict = static_cast<DictObj *>(d.v.obj());
    Root k1{ str_v("b") };
    if (dict_set(dict, Value::of_int(1), str_v("a")) != R::Ok ||
        dict_set(dict, k1.v, Value::of_int(2)) != R::Ok)
        return "inserting failed";
    // Insertion order, which is what makes printing a dict match CPython.
    Str bad = check_repr(d.v, "{1: 'a', 'b': 2}");
    if (!bad.empty())
        return bad;

    Value got;
    if (dict_get(dict, float_new(1.0), got) != R::Ok || str_of(got)->str() != "a")
        return "1.0 did not find the entry 1 made";
    if (dict_set(dict, Value::of_int(1), str_v("z")) != R::Ok || dict_len(dict) != 2)
        return "replacing a value changed the length";
    if (dict_del(dict, k1.v) != R::Ok || dict_len(dict) != 1)
        return "deleting did not take";
    if (dict_del(dict, k1.v) != R::NotImpl)
        return "deleting twice did not say the key was gone";
    if (py_getitem(d.v, k1.v, got) != R::Err || err_kind() != "KeyError")
        return "a missing key did not raise KeyError";
    err_clear();

    // Enough entries to rehash several times, then every one is checked.
    Root big{ obj_value(dict_new()) };
    DictObj *b = static_cast<DictObj *>(big.v.obj());
    for (i32 i = 0; i < 500; i++)
        if (dict_set(b, Value::of_int(i), Value::of_int(i * 2)) != R::Ok)
            return "the dict would not grow";
    if (dict_len(b) != 500)
        return why("entries", i64(dict_len(b)), 500);
    for (i32 i = 0; i < 500; i += 2)
        if (dict_del(b, Value::of_int(i)) != R::Ok)
            return "deleting from the big dict failed";
    if (dict_len(b) != 250)
        return why("entries after deleting half", i64(dict_len(b)), 250);
    for (i32 i = 1; i < 500; i += 2) {
        if (dict_get(b, Value::of_int(i), got) != R::Ok)
            return why("a surviving key went missing", i, i);
        i64 n = 0;
        if (!as_index(got, n) || n != i * 2)
            return why("a surviving value", n, i * 2);
    }

    // A tuple keys a dict; a list does not.
    Root key{ obj_value(tuple_new(1)) };
    static_cast<TupleObj *>(key.v.obj())->items()[0] = Value::of_int(9);
    if (dict_set(dict, key.v, Value::of_int(1)) != R::Ok)
        return "a tuple would not key a dict";
    Root bad_key{ obj_value(list_new()) };
    if (dict_set(dict, bad_key.v, Value::of_int(1)) != R::Err || err_kind() != "TypeError")
        return "a list keyed a dict";
    err_clear();
    return Str();
}

Str t_set()
{
    Root s{ obj_value(set_new()) };
    SetObj *set = static_cast<SetObj *>(s.v.obj());
    Str bad     = check_repr(s.v, "set()");
    if (!bad.empty())
        return bad;
    for (i32 i = 0; i < 3; i++)
        if (set_add(set, Value::of_int(i)) != R::Ok)
            return "adding failed";
    if (set_add(set, Value::of_int(1)) != R::Ok || set_len(set) != 3)
        return why("adding a duplicate", i64(set_len(set)), 3);

    bool has = false;
    if (set_has(set, Value::of_int(2), has) != R::Ok || !has)
        return "a member was not found";
    if (set_has(set, Value::of_int(9), has) != R::Ok || has)
        return "a non-member was found";
    if (set_discard(set, Value::of_int(1), has) != R::Ok || !has || set_len(set) != 2)
        return "discarding did not take";

    // Insertion order, not CPython's hash order: see README, known differences.
    return check_repr(s.v, "{0, 2}");
}

Str t_errors()
{
    err_clear();
    if (err_pending())
        return "the error channel did not clear";
    err_set2("TypeError", "unhashable type", "list");
    if (!err_pending() || err_kind() != "TypeError")
        return "the error channel did not take";
    if (err_message() != "unhashable type: list")
        return why_s("message", err_message(), "unhashable type: list");
    String formatted;
    err_format(formatted);
    if (formatted.str() != "TypeError: unhashable type: list")
        return why_s("formatted", formatted.str(), "TypeError: unhashable type: list");
    err_clear();

    Value out;
    if (py_binop(Value::of_int(1), str_v("a"), Op::Add, out) != R::Err)
        return "1 + 'a' did not fail";
    if (err_message() != "unsupported operand type(s) for +: 'int' and 'str'")
        return why_s("message", err_message(),
                     "unsupported operand type(s) for +: 'int' and 'str'");
    err_clear();
    return Str();
}

Str t_truth()
{
    Root empty{ obj_value(list_new()) }, full{ obj_value(list_new()) };
    list_push(list_of(full.v), Value::of_int(0));
    struct Case {
        Value v;
        bool want;
    };
    const Case CASES[] = {
        { value_none(), false },
        { value_bool(true), true },
        { value_bool(false), false },
        { Value::of_int(0), false },
        { Value::of_int(-1), true },
        { float_new(0.0), false },
        { float_new(0.5), true },
        { str_v(""), false },
        { str_v("x"), true },
        { empty.v, false },
        { full.v, true },
    };
    for (const Case &c : CASES)
        if (py_truth(c.v) != c.want)
            return why_s("truth", py_truth(c.v) ? "true" : "false", c.want ? "true" : "false");
    return Str();
}

// ------------------------------------------------------------------ phase 5

const CodeObj *find_code(const CodeObj *c, Str name)
{
    for (usize i = 0; i < c->consts.size(); i++) {
        if (!is_code(c->consts[i]))
            continue;
        const CodeObj *k = code_of(c->consts[i]);
        if (str_of(k->name)->str() == name)
            return k;
        if (const CodeObj *deeper = find_code(k, name))
            return deeper;
    }
    return nullptr;
}

// The reason, or empty with the module's code object pinned in `out`.
Str compile_to(Str source, Root &out)
{
    Ast ast;
    if (!ast.parse(source))
        return why_s("parse", err_message(), source);
    out = py_compile(ast, "<test>");
    return out.v.is_nil() ? why_s("compile", err_message(), source) : Str();
}

Str t_compile()
{
    Root code;
    Str bad = compile_to("x = 1 + 2\nprint(x)\n", code);
    if (!bad.empty())
        return bad;
    const CodeObj *c = code_of(code.v);

    constexpr Bc WANT[] = { Bc::LoadConst, Bc::LoadConst, Bc::BinaryOp, Bc::StoreName, Bc::LoadName,
                            Bc::LoadName,  Bc::Call,      Bc::PopTop,   Bc::LoadConst, Bc::Return };
    constexpr usize N   = sizeof(WANT) / sizeof(WANT[0]);
    if (c->code.size() != N)
        return why("instructions", i64(c->code.size()), i64(N));
    for (usize i = 0; i < N; i++)
        if (c->code[i].op != WANT[i])
            return why_s("opcode", bc_name(c->code[i].op), bc_name(WANT[i]));
    if (c->stacksize != 2)
        return why("stack", i64(c->stacksize), 2);
    if (c->names.size() != 2 || c->consts.size() != 3)
        return why("pools", i64(c->names.size() * 100 + c->consts.size()), 203);
    if (code_line(c, 0) != 1 || code_line(c, 4) != 2)
        return "the line table does not follow the source";
    return Str();
}

Str t_scopes()
{
    Root code;
    Str bad = compile_to(
        "def outer(a):\n"
        "    def inner():\n"
        "        return a\n"
        "    return inner\n",
        code);
    if (!bad.empty())
        return bad;
    const CodeObj *o = find_code(code_of(code.v), "outer");
    const CodeObj *n = find_code(code_of(code.v), "inner");
    if (!o || !n)
        return "the nested code objects are not in the constants";
    if (o->cellvars.size() != 1 || str_of(o->cellvars[0])->str() != "a")
        return "the captured parameter is not a cell";
    if (n->freevars.size() != 1 || str_of(n->freevars[0])->str() != "a")
        return "the inner scope does not hold a free variable";
    if (!(n->flags & CO_NESTED) || !(o->flags & CO_NEWLOCALS))
        return "the code object flags are wrong";
    // The cell parameter is copied out of its argument slot on entry.
    if (o->code.size() < 2 || o->code[0].op != Bc::LoadFast || o->code[1].op != Bc::StoreDeref)
        return "a cell parameter is not copied into its cell";
    return Str();
}

// ------------------------------------------------------------------ phase 7

Str t_exceptions()
{
    // Every name the error channel can raise has to be in the table, or an
    // `except` could never catch it.
    constexpr Str RAISED[] = { "AttributeError",    "ImportError",       "IndexError",
                               "KeyError",          "MemoryError",       "NameError",
                               "OverflowError",     "RecursionError",    "SyntaxError",
                               "SystemError",       "TypeError",         "ValueError",
                               "ZeroDivisionError", "UnboundLocalError", "KeyboardInterrupt",
                               "StopIteration",     "SystemExit" };
    for (Str n : RAISED)
        if (!exc_find(n))
            return why_s("missing exception type", n, "in the table");

    const ExcType *base = exc_find("BaseException");
    const ExcType *any  = exc_find("Exception");
    const ExcType *zero = exc_find("ZeroDivisionError");
    if (!exc_is(zero, exc_find("ArithmeticError")) || !exc_is(zero, any) || !exc_is(zero, base))
        return "ZeroDivisionError is not under ArithmeticError, Exception and BaseException";
    if (exc_is(any, zero) || exc_is(exc_find("SystemExit"), any))
        return "the hierarchy runs the wrong way";

    // Every row has to reach BaseException, or matching would loop.
    for (usize i = 0; i < EXC_COUNT; i++)
        if (!exc_is(&EXC_TABLE[i], base))
            return why_s("not under BaseException", EXC_TABLE[i].name, "it should be");

    Root e{ exc_make("ValueError", "bad") };
    if (e.v.is_nil() || exc_type_of(e.v) != exc_find("ValueError"))
        return "exc_make did not build a ValueError";
    String line;
    if (!exc_line(e.v, line) || line.str() != "ValueError: bad")
        return why_s("the last traceback line", line.str(), "ValueError: bad");
    return check_repr(e.v, "ValueError('bad')");
}

struct Case {
    Str name;
    Str (*run)();
};

constexpr Case CASES[] = {
    { "value", t_value },
    { "strings", t_strings },
    { "intern", t_intern },
    { "collect", t_collect },
    { "root", t_root },
    { "tuple", t_tuple },
    { "list", t_list },
    { "cycle", t_cycle },
    { "deep", t_deep },
    { "stress", t_stress },
    { "threshold", t_threshold },
    { "numbers", t_numbers },
    { "floats", t_floats },
    { "compare", t_compare },
    { "strtext", t_strtext },
    { "reprs", t_reprs },
    { "dict", t_dict },
    { "set", t_set },
    { "errors", t_errors },
    { "truth", t_truth },
    { "compile", t_compile },
    { "scopes", t_scopes },
    { "exceptions", t_exceptions },
};

} // namespace

bool selftest_run(String &out)
{
    py_init();

    usize passed = 0;
    for (const Case &c : CASES) {
        err_clear();
        Str reason = c.run();
        // Leave nothing behind: the next case counts live objects too.
        gc_stress(false);
        err_clear();
        gc_collect();
        if (reason.empty()) {
            passed++;
            out.append("ok ");
            out.append(c.name);
            out.push('\n');
        } else {
            out.append("FAIL ");
            out.append(c.name);
            out.append(": ");
            out.append(reason);
            out.push('\n');
        }
    }

    usize total = sizeof(CASES) / sizeof(CASES[0]);
    Buf<64> tail;
    tail.put("selftest: ").put(u64(passed)).put(" of ").put(u64(total)).put('\n');
    out.append(tail.str());
    return passed == total;
}
