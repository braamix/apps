// `python --selftest`: the object heap and the collector, checked from inside.
//
// Nothing here is Python-visible, so there is no source to run and no output to
// compare against CPython's. These are the assertions instead, and test/pygc.mjs
// reads what they print.
#include "selftest.h"

#include "gc.h"
#include "intern.h"
#include "kernel/fmt.h"
#include "obj.h"

namespace {

// A reason with numbers in it. Valid until the next call, which is why every
// caller returns it straight away.
Buf<96> detail;

Str why(Str what, i64 got, i64 want)
{
    detail.clear();
    detail.put(what).put(": got ").put(u64(got < 0 ? -got : got));
    detail.put(", want ").put(u64(want < 0 ? -want : want));
    return detail.str();
}

usize live()
{
    return gc_stats().objects;
}

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
    Root a{ obj_value(str_new("hello")) };
    Root b{ obj_value(str_new("hello")) };
    if (a.v.is_nil() || b.v.is_nil())
        return "str_new returned nothing";
    StrObj *x = static_cast<StrObj *>(a.obj());
    StrObj *y = static_cast<StrObj *>(b.obj());
    if (x->str() != "hello")
        return "the bytes did not survive the copy";
    if (x == y)
        return "str_new returned the same object twice";
    if (x->hash != y->hash)
        return "equal bytes hash differently";
    if (str_new("")->str().size() != 0)
        return "the empty string is not empty";
    return Str();
}

Str t_intern()
{
    usize before  = intern_count();
    StrObj *first = str_intern("spam");
    StrObj *again = str_intern("spam");
    if (!first || first != again)
        return "the same bytes gave two objects";
    if (str_intern("eggs") == first)
        return "different bytes gave one object";
    if (intern_count() != before + 2)
        return why("interned", i64(intern_count() - before), 2);

    // The table is a root, so a collection with nothing else holding them
    // leaves both alive and readable.
    gc_collect();
    if (str_intern("spam") != first || first->str() != "spam")
        return "an interned string did not survive a collection";
    return Str();
}

Str t_collect()
{
    usize base = live();
    for (usize i = 0; i < 100; i++)
        if (!str_new("garbage"))
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
        Root r{ obj_value(str_new("kept")) };
        for (usize i = 0; i < 50; i++)
            str_new("dropped");
        gc_collect();
        if (live() != base + 1)
            return why("survivors", i64(live() - base), 1);
        if (static_cast<StrObj *>(r.obj())->str() != "kept")
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
    TupleObj *tup = static_cast<TupleObj *>(t.obj());
    for (usize i = 0; i < 3; i++)
        if (tup->items()[i] != Value())
            return "a fresh tuple is not all Nil";

    tup->items()[0] = obj_value(str_new("a"));
    tup->items()[1] = Value::of_int(7);
    tup->items()[2] = obj_value(tuple_new(0));
    gc_collect();
    if (live() != base + 3)
        return why("the tuple and what it holds", i64(live() - base), 3);
    if (static_cast<StrObj *>(tup->items()[0].obj())->str() != "a")
        return "the traced string was not itself afterwards";
    return Str();
}

Str t_list()
{
    usize base = live();
    Root l{ obj_value(list_new()) };
    ListObj *list = static_cast<ListObj *>(l.obj());
    for (usize i = 0; i < 1000; i++) {
        StrObj *s = str_new("item");
        if (!s || !list_push(list, obj_value(s)))
            return "the list would not grow";
    }
    gc_collect();
    if (live() != base + 1001)
        return why("the list and its items", i64(live() - base), 1001);
    if (list->items.size() != 1000)
        return why("items", i64(list->items.size()), 1000);
    if (static_cast<StrObj *>(list->items[999].obj())->str() != "item")
        return "the last item was not itself afterwards";
    return Str();
}

Str t_cycle()
{
    usize base = live();
    {
        ListObj *a = list_new();
        Root ra{ obj_value(a) };
        ListObj *b = list_new();
        list_push(a, obj_value(b));
        list_push(b, obj_value(a)); // a -> b -> a
        list_push(a, obj_value(a)); // and a -> a
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
        ListObj *at = static_cast<ListObj *>(head.obj());
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
    ListObj *list = static_cast<ListObj *>(l.obj());
    for (usize i = 0; i < 200; i++) {
        // Every one of these allocations collects first. Anything reachable
        // only from a C++ local would be gone by the next line.
        TupleObj *t = tuple_new(2);
        Root rt{ obj_value(t) };
        t->items()[0] = obj_value(str_intern("pinned"));
        t->items()[1] = obj_value(str_new("fresh"));
        if (!list_push(list, obj_value(t)))
            return "the list would not grow";
    }
    gc_stress(false);
    if (gc_stats().collections <= swept + 200)
        return "stress did not collect at every allocation";
    // The list, 200 tuples, 200 fresh strings, and "pinned" interned once.
    if (live() != base + 402)
        return why("what the stressed loop built", i64(live() - base), 402);
    for (usize i = 0; i < 200; i++) {
        TupleObj *t = static_cast<TupleObj *>(list->items[i].obj());
        if (static_cast<StrObj *>(t->items()[0].obj())->str() != "pinned" ||
            static_cast<StrObj *>(t->items()[1].obj())->str() != "fresh")
            return "an item did not survive the stress intact";
    }
    return Str();
}

Str t_threshold()
{
    usize before = gc_stats().collections;
    // No gc_collect() here: enough garbage to cross the threshold on its own.
    for (usize i = 0; i < 20000; i++)
        if (!str_new("0123456789012345678901234567890123456789"))
            return "allocation failed";
    if (gc_stats().collections == before)
        return "the threshold never fired";
    if (gc_stats().freed == 0)
        return "nothing was swept";
    return Str();
}

struct Case {
    Str name;
    Str (*run)();
};

constexpr Case CASES[] = {
    { "value", t_value },     { "strings", t_strings },     { "intern", t_intern },
    { "collect", t_collect }, { "root", t_root },           { "tuple", t_tuple },
    { "list", t_list },       { "cycle", t_cycle },         { "deep", t_deep },
    { "stress", t_stress },   { "threshold", t_threshold },
};

} // namespace

bool selftest_run(String &out)
{
    py_init();

    usize passed = 0;
    for (const Case &c : CASES) {
        Str reason = c.run();
        // Leave nothing behind: the next case counts live objects too.
        gc_stress(false);
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
