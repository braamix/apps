// The object heap and its collector: precise mark and sweep.
//
// Conservative scanning is not available on this target -- there is no
// __builtin_frame_address, no exported stack base, and wasm keeps pointers in
// locals that a scan of linear memory cannot see. So every root is explicit.
// C++ that holds an object across an allocation must pin it:
//
//     Root s{ obj_value(str_new("x")) };   // survives the tuple below
//     TupleObj *t = tuple_new(1);
//
// Forget the Root and the string is freed under you. That is the price of a
// collector that reclaims cycles, which Python makes constantly.
#pragma once

#include "value.h"

struct Obj;
struct ListObj;

// A run of values the collector must treat as reachable. Intrusive, so a pin
// costs three stores and no allocation. Stack discipline: destroyed in reverse.
struct Roots {
    Value *p;
    usize n;
    Roots *prev;

    Roots(Value *p_, usize n_);
    ~Roots();

    Roots(const Roots &)            = delete;
    Roots &operator=(const Roots &) = delete;
};

// One pinned value. `v` is declared first so it is live before the pin sees it.
struct Root {
    Value v;
    Roots pin;

    explicit Root(Value x = Value()) : v(x), pin(&v, 1) {}

    Root &operator=(Value x)
    {
        v = x;
        return *this;
    }

    operator Value() const { return v; }

    Obj *obj() const { return v.obj(); }
};

// Reachable, and its children with it. Safe on Nil and on small integers.
void gc_mark(Value v);

// Mark from every root, then free what was not marked.
void gc_collect();

// Where the VM's stacks, the builtins and anything else that outlives one
// operation join the root set. Idempotent: registering the same function twice
// adds it once.
void gc_root_hook(void (*f)());

// Collect at every allocation. Slow, and it turns a missing Root into a
// failure the selftest can see.
void gc_stress(bool on);

// The automatic collection at an allocation, which `gc.disable()` turns off.
// An explicit gc_collect() still runs.
void gc_enable(bool on);
bool gc_enabled();

// The bytes allocated since the last collection, and the pressure at which the
// next one happens. `gc.get_threshold` and `gc.set_threshold` are these, in
// bytes rather than CPython's generation counts.
usize gc_pressure();
usize gc_threshold();
void gc_set_threshold(usize bytes);

// Every live object, as a fresh list. Null with the error pending.
ListObj *gc_objects();

struct GcStats {
    usize objects;     // live, immortals included
    usize bytes;       // what those objects hold, header and payload
    usize collections; // since the process started
    usize freed;       // objects swept, over all collections
};

GcStats gc_stats();

// Put a static object under the collector's eye without ever freeing it.
void gc_immortal(Obj *o);

// ------------------------------------------------------- finalizers

// Run after marking and before sweeping, where what is about to be freed is
// still whole: this is where a weak reference is cleared and its callback
// owed. Idempotent, like gc_root_hook.
void gc_sweep_hook(void (*f)());

// Inside a sweep hook: `o` is unreachable and this collection will free it.
bool gc_is_dying(Obj *o);

// A call the collector owes. `fn` Nil means the object's own __del__; anything
// else is called with the object as its one argument. Both are roots until the
// VM makes the call, which is also what keeps the object alive for it -- so a
// __del__ that stores `self` somewhere resurrects it, and is never owed again.
bool gc_defer(Value obj, Value fn);

bool gc_owes();

// The oldest owed call. False when there is none.
bool gc_take(Value &obj, Value &fn);
