// The struct sequence: a tuple whose fields also have names.
//
// `sys.version_info`, `sys.float_info` and `time.struct_time` are all this.
// CPython builds one type per module in C; here one layout serves them all and
// the static Type each points at supplies the name its repr prints, so
// INFO_TYPE declares a descriptor and info_new fills an object.
//
// It is not a tuple subclass, as CPython's is, so the comparisons are written
// out: `sys.version_info >= (3, 0)` compares against a plain tuple because
// info_eq and info_order take one either side.
#pragma once

#include "obj.h"

struct InfoObj : Obj {
    Value items;  // TupleObj: the fields an index reaches
    Value names;  // TupleObj of StrObj: those, then the hidden ones
    Value hidden; // TupleObj: fields only a name reaches
};

void info_trace(Obj *o);
R info_len(Value v, usize &out);
R info_getitem(Value v, Value key, Value &out);
R info_getattr(Value v, StrObj *name, Value &out);
R info_eq(Value a, Value b, bool &out);
R info_order(Value a, Value b, Cmp op, bool &out);
R info_contains(Value v, Value item, bool &out);
R info_hash(Value v, u32 &out);
Value info_iter(Value v);
R info_repr(Value v, String &out);

// One descriptor per named tuple, so each prints under its own name.
#define INFO_TYPE(var, label)                                                                      \
    constexpr Type var                                                                             \
    {                                                                                              \
        .name = label, .trace = info_trace, .hash = info_hash, .eq = info_eq, .order = info_order, \
        .repr = info_repr, .len = info_len, .getitem = info_getitem, .contains = info_contains,    \
        .iter = info_iter, .getattr = info_getattr                                                 \
    }

// `items` and `names` are `n` long; the last `n - shown` are reached by name
// alone, as sys.flags.gil is. Nil with the error pending.
Value info_new(const Type *t, const Value *items, const Str *names, usize n, usize shown);

// `t(sequence, dict=None)`, as CPython's structseq_new: at least `shown`
// items and at most `n`, the rest from `dict` by name or None.
struct CallArgs;
R info_construct(const Type *t, const Str *names, usize n, usize shown, const CallArgs &a,
                 Value &out);

inline Value info_new(const Type *t, const Value *items, const Str *names, usize n)
{
    return info_new(t, items, names, n, n);
}

// __reduce__: (type, (the fields an index reaches, {the hidden ones})), which
// is what `t(sequence, dict)` makes again. Nil with the error pending.
Value info_reduce(Value v);

inline bool is_info(Value v)
{
    return v.is_obj() && v.obj()->type->trace == info_trace;
}

// The fields an index reaches, as a TupleObj. CPython makes a struct sequence
// a tuple subclass, so what unpacks a tuple should unpack one of these.
inline Value info_items(Value v)
{
    return static_cast<InfoObj *>(v.obj())->items;
}
