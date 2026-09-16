// The object header, the type descriptor and its protocol slots, and the
// objects themselves.
#pragma once

#include "err.h"
#include "kernel/str.h"
#include "kernel/string.h"
#include "kernel/vec.h"
#include "value.h"

struct Obj;
struct StrObj;

enum class Cmp : u8 { Eq, Ne, Lt, Le, Gt, Ge, In, NotIn, Is, IsNot };

// The binary operators the number and sequence protocols answer.
enum class Op : u8 { Add, Sub, Mul, Div, FloorDiv, Mod, Pow, And, Or, Xor, Lsh, Rsh };

// The unary ones. Here rather than in the parser, because the bytecode and the
// VM name them too.
enum class Un : u8 { Invert, Not, UAdd, USub };

Str op_symbol(Op op);
Str cmp_symbol(Cmp op);

// A static descriptor, one per type; types become objects in phase 9. Every
// slot may be null, and the generic operation in ops.h says what that means.
struct Type {
    Str name;
    // The TypeObj this descriptor belongs to, once something has asked for it.
    // Null until then; a `class` fills it in at birth, which is also what says
    // an instance of it is a class instance rather than a built-in.
    Obj *owner           = nullptr;
    void (*trace)(Obj *) = nullptr; // call gc_mark on every value held
    void (*fini)(Obj *)  = nullptr; // release any heap block held, before free

    bool (*truth)(Value)                        = nullptr; // null: always true
    R (*hash)(Value, u32 &out)                  = nullptr; // null: unhashable
    R (*eq)(Value, Value, bool &out)            = nullptr; // null: identity
    R (*order)(Value, Value, Cmp, bool &out)    = nullptr; // null: unorderable
    R (*repr)(Value, String &out)               = nullptr;
    R (*str)(Value, String &out)                = nullptr; // null: use repr
    R (*len)(Value, usize &out)                 = nullptr;
    R (*getitem)(Value, Value key, Value &out)  = nullptr;
    R (*setitem)(Value, Value key, Value v)     = nullptr;
    R (*delitem)(Value, Value key)              = nullptr;
    R (*contains)(Value, Value item, bool &out) = nullptr;
    R (*binop)(Value, Value, Op, Value &out)    = nullptr; // this type either side

    Value (*iter)(Value)                          = nullptr; // Nil on error
    R (*next)(Value, Value &out)                  = nullptr; // NotImpl at the end
    R (*getattr)(Value, StrObj *name, Value &out) = nullptr; // NotImpl: no such
    R (*setattr)(Value, StrObj *name, Value v)    = nullptr; // null: immutable
};

// Sixteen bytes, the smallest size class. `next` is the heap list, which
// kernel/alloc.h cannot walk for us; `grey` is the marker's worklist.
struct Obj {
    const Type *type;
    Obj *next;
    Obj *grey;
    u32 flags;
};

enum : u32 {
    OBJ_MARK      = 1u << 0, // reachable, this collection
    OBJ_GREY      = 1u << 1, // on the marker's worklist
    OBJ_IMMORTAL  = 1u << 2, // static storage: never swept, never freed
    OBJ_ASCII     = 1u << 3, // a str whose bytes are all under 0x80
    OBJ_EXC       = 1u << 4, // an ExcObj, whatever class it belongs to
    OBJ_TYPE      = 1u << 5, // a TypeObj, whatever metaclass it belongs to
    OBJ_FINAL     = 1u << 6, // its class has a __del__ the sweep owes a call
    OBJ_FINALIZED = 1u << 7, // that call has been owed once, and never is again
};

// Allocate `bytes` (header included) and thread it onto the heap list. Null on
// OOM. May collect first, so pin anything live across the call.
Obj *obj_alloc(const Type *t, usize bytes);

inline Value obj_value(Obj *o)
{
    return o ? Value::of_obj(o) : Value();
}

// The type of any value, small integers included. Null only for Nil.
const Type *type_of(Value v);

Str type_name(Value v);

// Register the singletons with the collector. Idempotent.
void py_init();

// ---------------------------------------------------------------- singletons

extern Obj none_obj;
extern Obj true_obj;
extern Obj false_obj;
extern Obj ellipsis_obj;
extern Obj notimpl_obj;

extern const Type none_type;
extern const Type bool_type;
extern const Type ellipsis_type;
extern const Type notimpl_type;
extern const Type int_type;
extern const Type float_type;
extern const Type str_type;
extern const Type bytes_type;
extern const Type tuple_type;
extern const Type list_type;
extern const Type dict_type;
extern const Type set_type;
extern const Type bytearray_type;
extern const Type frozenset_type;

inline Value value_none()
{
    return Value::of_obj(&none_obj);
}

inline Value value_bool(bool b)
{
    return Value::of_obj(b ? &true_obj : &false_obj);
}

inline Value value_ellipsis()
{
    return Value::of_obj(&ellipsis_obj);
}

inline Value value_notimpl()
{
    return Value::of_obj(&notimpl_obj);
}

// A binary special method saying it is the other operand's turn.
inline bool is_notimpl(Value v)
{
    return v.w == Value::of_obj(&notimpl_obj).w;
}

inline bool is_none(Value v)
{
    return v.w == Value::of_obj(&none_obj).w;
}

inline bool is_bool(Value v)
{
    return v.is_obj() && v.obj()->type == &bool_type;
}

inline bool is_true(Value v)
{
    return v.w == Value::of_obj(&true_obj).w;
}

// ------------------------------------------------------------------ numbers

// Nil on overflow, with OverflowError pending: there is no bignum yet.
Value int_from_i64(i64 n);

// The decimal of a signed integer, into `out`; the Str may point into it.
// kernel/fmt.h's Buf has no signed 64-bit put, and every caller wants one.
Str int_text(char *out, usize cap, i64 v);

struct FloatObj : Obj {
    f64 v;
};

Value float_new(f64 v);

// CPython's float repr, into `out`; the Str may point into it.
Str float_text(char *out, usize cap, f64 v);

inline bool is_float(Value v)
{
    return v.is_obj() && v.obj()->type == &float_type;
}

inline f64 float_of(Value v)
{
    return static_cast<FloatObj *>(v.obj())->v;
}

// True for int and bool, not for float; `out` takes the value.
bool as_index(Value v, i64 &out);

// int, bool or float, widened.
bool as_number(Value v, f64 &out);

// ---------------------------------------------------------------- str, bytes

// Bytes inline after the header; immutable, so hash and chars are cached.
struct StrObj : Obj {
    u32 len;
    u32 chars;
    u32 hash;

    char *bytes() { return reinterpret_cast<char *>(this) + sizeof(StrObj); }

    const char *bytes() const { return reinterpret_cast<const char *>(this) + sizeof(StrObj); }

    Str str() const { return Str(bytes(), len); }
};

// Nil on bad UTF-8, with ValueError pending.
Value str_new(Str s);

// The bytes are known good: no validation and no error.
StrObj *str_raw(Str s);

inline bool is_str(Value v)
{
    return v.is_obj() && v.obj()->type == &str_type;
}

inline StrObj *str_of(Value v)
{
    return static_cast<StrObj *>(v.obj());
}

// The codepoint at character index i, and the byte offset of that character.
u32 str_char_at(const StrObj *s, usize i);
usize str_offset_of(const StrObj *s, usize i);

struct BytesObj : Obj {
    u32 len;
    u32 hash;

    u8 *data() { return reinterpret_cast<u8 *>(this) + sizeof(BytesObj); }

    const u8 *data() const { return reinterpret_cast<const u8 *>(this) + sizeof(BytesObj); }

    Str str() const { return Str(reinterpret_cast<const char *>(data()), len); }
};

Value bytes_new(Str s);

inline bool is_bytes(Value v)
{
    return v.is_obj() && v.obj()->type == &bytes_type;
}

// bytearray: the same octets, growable. A Vec, since it is resized.
struct ArrayObj : Obj {
    Vec<u8> data;

    Str str() const { return Str(reinterpret_cast<const char *>(data.data()), data.size()); }
};

Value bytearray_new(Str s);

inline bool is_bytearray(Value v)
{
    return v.is_obj() && v.obj()->type == &bytearray_type;
}

inline ArrayObj *array_of(Value v)
{
    return static_cast<ArrayObj *>(v.obj());
}

// ------------------------------------------------------------- tuple, list

// Values stored inline after the header.
struct TupleObj : Obj {
    u32 len;

    Value *items() { return reinterpret_cast<Value *>(this + 1); }

    const Value *items() const { return reinterpret_cast<const Value *>(this + 1); }
};

// Every item Nil, for the caller to fill; a trace between the two is safe.
TupleObj *tuple_new(usize n);

inline bool is_tuple(Value v)
{
    return v.is_obj() && v.obj()->type == &tuple_type;
}

struct ListObj : Obj {
    Vec<Value> items;
};

ListObj *list_new();
bool list_push(ListObj *l, Value v);

inline bool is_list(Value v)
{
    return v.is_obj() && v.obj()->type == &list_type;
}

inline ListObj *list_of(Value v)
{
    return static_cast<ListObj *>(v.obj());
}

// ---------------------------------------------------------------- dict, set

// One slot of the table dict and set share; a set leaves `val` Nil.
struct Entry {
    Value key;
    Value val;
    u32 hash;
};

// `entries` is insertion order, `index` the hash table over it. The order is
// what makes printing a dict match CPython, which has kept it since 3.7.
struct Table {
    Vec<Entry> entries;
    Vec<i32> index;
    usize live;
};

struct DictObj : Obj {
    Table t;
};

struct SetObj : Obj {
    Table t;
};

DictObj *dict_new();
R dict_get(DictObj *d, Value key, Value &out); // NotImpl when absent
R dict_set(DictObj *d, Value key, Value val);
R dict_del(DictObj *d, Value key); // NotImpl when absent

inline bool is_dict(Value v)
{
    return v.is_obj() && v.obj()->type == &dict_type;
}

inline bool is_set(Value v)
{
    return v.is_obj() && v.obj()->type == &set_type;
}

// frozenset has set's layout and different slots.
inline bool is_frozenset(Value v)
{
    return v.is_obj() && v.obj()->type == &frozenset_type;
}

inline bool is_anyset(Value v)
{
    return is_set(v) || is_frozenset(v);
}

inline SetObj *set_at(Value v)
{
    return static_cast<SetObj *>(v.obj());
}

inline usize dict_len(const DictObj *d)
{
    return d->t.live;
}

SetObj *set_new();
SetObj *frozenset_new();
R set_add(SetObj *s, Value v);
R set_has(SetObj *s, Value v, bool &out);
R set_discard(SetObj *s, Value v, bool &out);

inline usize set_len(const SetObj *s)
{
    return s->t.live;
}

// Walk the live entries in order. Start with `at` zero; false when done.
bool table_next(const Table &t, usize &at, Value &key, Value &val);
