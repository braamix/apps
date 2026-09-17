// Methods on the built-in types: a static table per type, installed into the
// TypeObj that wraps it.
//
// A method is a native whose first argument is self. Phase 9 already binds a
// native reached through an instance, so `[].append` is a MethodObj over one
// and costs no new kind of object. type_lookup finds the same native for a
// subclass, so every method takes its self through method_self.
#pragma once

#include "type.h"

struct Method {
    Str name;
    R (*fn)(const CallArgs &, Value &out);
    bool stat = false; // a static method: str.maketrans, int.from_bytes
};

// Fill the TypeObj wrapping `t` from `tab`.
bool method_install(const Type *t, const Method *tab, usize n);

template <usize N>
inline bool method_install(const Type *t, const Method (&tab)[N])
{
    return method_install(t, tab, N);
}

// Every built-in type's methods. Called once, from builtins_dict().
bool methods_install();

// What a built-in method acts on: a subclass instance stands for the built-in
// laid out inside it.
Value method_self(Value v);

// `name` as a method of `v`'s type, bound to `v`. NotImpl when there is none.
R method_find(Value v, StrObj *name, Value &out);

// The argument check every method makes. `least` and `most` do not count self,
// and neither does the call the program wrote.
bool meth_args(const CallArgs &a, Str who, u32 least, u32 most);

// The same, for a method that takes keywords. `names` is the parameter list
// after self, `least` how many are required. `out` takes one value each, Nil
// where the call left it out.
bool meth_take(const CallArgs &a, Str who, const Str *names, u32 n, u32 least, Value *out);

template <usize N>
inline bool meth_take(const CallArgs &a, Str who, const Str (&names)[N], u32 least, Value (&out)[N])
{
    return meth_take(a, who, names, N, least, out);
}

// A method's self, checked. Null with a TypeError pending.
StrObj *self_str(const CallArgs &a, Str who);
ListObj *self_list(const CallArgs &a, Str who);
DictObj *self_dict(const CallArgs &a, Str who);
DictObj *self_anydict(const CallArgs &a, Str who); // a frozendict too
SetObj *self_set(const CallArgs &a, Str who, bool frozen_ok = true);
TupleObj *self_tuple(const CallArgs &a, Str who);

R oom_err();

// The stable merge `sorted` and `list.sort` share. `idx` is permuted so that
// `keys[idx[i]]` ascends, or descends when `rev`. Iterative: a recursive sort
// is a risk on a 128 KiB stack.
R sort_idx(const Vec<Value> &keys, Vec<u32> &idx, bool rev);

// ------------------------------------------------------------ the new types

// The octets of a bytes, a bytearray or a memoryview. False for anything else.
// This is the whole buffer protocol here.
bool bytes_like(Value v, Str &out);

// memoryview: a window on another object's octets, not a copy.
extern const Type memview_type;

Value memview_new(Value owner);

inline bool is_memview(Value v)
{
    return v.is_obj() && v.obj()->type == &memview_type;
}

// The window's octets, and whether it may be written through.
bool memview_bytes(Value v, Str &out, bool *writable = nullptr);

// A fresh str with no validation, or Nil with MemoryError pending.
Value str_of_bytes(Str s);

// Each table's install, defined beside the methods it installs.
bool str_methods();
bool bytes_methods();
bool seq_methods();
bool map_methods();
bool num_methods();
bool code_methods();

// The protocol methods -- __len__, __getitem__, __eq__ -- in each built-in
// type's own namespace, over the slots that type fills. Last, so a method a
// table already named wins. slotmeth.cpp.
bool slot_methods();

// ------------------------------------------------------------ the iterators

// reversed(seq): backwards over anything with a len and an integer getitem.
Value reversed_new(Value seq);

// zip(*iters): lazy, stopping at the shortest.
Value zip_new(Value iters, bool strict = false);

// An iterator over a list already built, under a borrowed name. This is what
// the eager map() and filter() hand back; see README.
Value made_iter(Value list, const Type *t);

extern const Type map_type;
extern const Type filter_type;

// The keys, values and items of a dict, as their own iterable types.
enum : u32 { VIEW_KEYS, VIEW_VALUES, VIEW_ITEMS };

extern const Type view_type;

Value dict_view(Value d, u32 kind);

// The set protocol: set, frozenset and the three views all answer it. Defined
// beside the set methods, named here so table.cpp can wire the slots.
R anyset_eq(Value a, Value b, bool &out);
R anyset_order(Value a, Value b, Cmp op, bool &out);
R anyset_binop(Value a, Value b, Op op, Value &out);
R frozenset_hash(Value v, u32 &out);
