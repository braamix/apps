// The object header, the type descriptor, and the objects phase 1 needs to
// have something to collect. Phase 2 fills these types out; here they exist,
// trace and die.
#pragma once

#include "kernel/str.h"
#include "kernel/vec.h"
#include "value.h"

struct Obj;

// A static descriptor, one per type. Not itself an object: types become
// Python-visible in phase 9, and until then there is no metatype to want.
struct Type {
    Str name;
    void (*trace)(Obj *) = nullptr; // call gc_mark on every value held
    void (*fini)(Obj *)  = nullptr; // release any heap block held, before free
};

// Sixteen bytes, which is the smallest size class. `next` threads every object
// onto one list, because kernel/alloc.h has no heap iterator; `grey` threads
// the marker's worklist, so marking neither allocates nor recurses.
struct Obj {
    const Type *type;
    Obj *next;
    Obj *grey;
    u32 flags;
};

enum : u32 {
    OBJ_MARK     = 1u << 0, // reachable, this collection
    OBJ_GREY     = 1u << 1, // on the marker's worklist
    OBJ_IMMORTAL = 1u << 2, // static storage: never swept, never freed
};

// Allocate `bytes` (header included) and thread it onto the heap list. Null on
// OOM. May collect first, so pin anything live across the call.
Obj *obj_alloc(const Type *t, usize bytes);

inline Value obj_value(Obj *o)
{
    return o ? Value::of_obj(o) : Value();
}

// The type of any value, small integers included.
const Type *type_of(Value v);

// Register the singletons with the collector. Idempotent.
void py_init();

// ---------------------------------------------------------------- singletons

extern Obj none_obj;
extern Obj true_obj;
extern Obj false_obj;

extern const Type none_type;
extern const Type bool_type;
extern const Type int_type;
extern const Type str_type;
extern const Type tuple_type;
extern const Type list_type;

inline Value value_none()
{
    return Value::of_obj(&none_obj);
}

inline Value value_bool(bool b)
{
    return Value::of_obj(b ? &true_obj : &false_obj);
}

// ------------------------------------------------------------------- objects

// Bytes stored inline after the header; immutable, so the hash is cached.
struct StrObj : Obj {
    u32 len;
    u32 hash;

    char *bytes() { return reinterpret_cast<char *>(this) + sizeof(StrObj); }

    const char *bytes() const { return reinterpret_cast<const char *>(this) + sizeof(StrObj); }

    Str str() const { return Str(bytes(), len); }
};

StrObj *str_new(Str s);

// Values stored inline after the header.
struct TupleObj : Obj {
    u32 len;

    Value *items() { return reinterpret_cast<Value *>(this + 1); }

    const Value *items() const { return reinterpret_cast<const Value *>(this + 1); }
};

// Every item Nil, for the caller to fill; a trace between the two is safe.
TupleObj *tuple_new(usize n);

struct ListObj : Obj {
    Vec<Value> items;
};

ListObj *list_new();
bool list_push(ListObj *l, Value v);
