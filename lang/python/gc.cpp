// Mark and sweep over the heap list.
#include "gc.h"

#include "err.h"
#include "intern.h"
#include "kernel/alloc.h"
#include "kernel/host.h"
#include "obj.h"

namespace {

// All PODs: a namespace-scope global here must be trivially destructible,
// __cxa_atexit not existing.
Obj *all     = nullptr; // every heap object, newest first
Obj *grey    = nullptr; // the marker's worklist
Roots *roots = nullptr; // the innermost pin

// A handful of root providers, because more than one subsystem outlives an
// operation: the VM's frames, the builtins namespace, the type objects, the
// exception types and the module cache.
constexpr usize MAX_HOOKS = 8;
void (*hooks[MAX_HOOKS])();
usize nhooks = 0;

void (*sweepers[MAX_HOOKS])();
usize nsweepers = 0;

// The calls the collector owes. A Vec cannot be a namespace-scope object here,
// so it is built on first use and never torn down.
struct Owed {
    Value obj, fn;
};

Vec<Owed> *owed;
usize owed_at = 0;

usize live_objects = 0;
usize live_bytes   = 0;
usize since_gc     = 0;
usize threshold    = 64 * 1024;
usize collections  = 0;
usize freed_total  = 0;
bool stress        = false;
bool collecting    = false;

constexpr usize THRESHOLD_MIN = 64 * 1024;

void sweep()
{
    Obj **link = &all;
    for (Obj *o = all; o;) {
        Obj *next = o->next;
        if (o->flags & (OBJ_MARK | OBJ_IMMORTAL)) {
            o->flags &= ~u32(OBJ_MARK | OBJ_GREY);
            link = &o->next;
        } else {
            *link = next;
            live_objects--;
            // Immortals are static storage, so their size is never asked for:
            // heap_usable_size traps on anything that is not a live block.
            live_bytes -= heap_usable_size(o);
            if (o->type->fini)
                o->type->fini(o);
            heap_free(o);
            freed_total++;
        }
        o = next;
    }
}

} // namespace

Roots::Roots(Value *p_, usize n_) : p(p_), n(n_), prev(roots)
{
    roots = this;
}

Roots::~Roots()
{
    roots = prev;
}

void gc_mark(Value v)
{
    if (!v.is_obj())
        return;
    Obj *o = v.obj();
    if (o->flags & OBJ_MARK)
        return;
    o->flags |= OBJ_MARK | OBJ_GREY;
    o->grey = grey;
    grey    = o;
}

// The worklist is a chain through the objects themselves, so a structure ten
// thousand deep costs no native stack and no allocation.
void drain()
{
    while (grey) {
        Obj *o = grey;
        grey   = o->grey;
        o->flags &= ~u32(OBJ_GREY);
        o->grey = nullptr;
        if (o->type->trace)
            o->type->trace(o);
    }
}

void gc_collect()
{
    collecting = true;
    collections++;

    for (Roots *r = roots; r; r = r->prev)
        for (usize i = 0; i < r->n; i++)
            gc_mark(r->p[i]);
    intern_mark();
    err_mark();
    for (usize i = 0; i < nhooks; i++)
        hooks[i]();
    if (owed)
        for (usize i = owed_at; i < owed->size(); i++) {
            gc_mark((*owed)[i].obj);
            gc_mark((*owed)[i].fn);
        }
    drain();

    // What is about to go is still whole here: a weak reference to it is
    // cleared now, and its callback owed, before any finalizer can see it.
    for (usize i = 0; i < nsweepers; i++)
        sweepers[i]();

    // An object whose class writes __del__ is owed one call, ever. Owing it
    // marks the object, so this collection leaves it -- and everything it
    // reaches -- alone; the next one frees it unless the call kept it.
    for (Obj *o = all; o; o = o->next)
        if (!(o->flags & (OBJ_MARK | OBJ_IMMORTAL)) && (o->flags & OBJ_FINAL) &&
            !(o->flags & OBJ_FINALIZED)) {
            o->flags |= OBJ_FINALIZED;
            gc_defer(Value::of_obj(o), Value());
        }
    drain();

    sweep();
    since_gc   = 0;
    threshold  = live_bytes > THRESHOLD_MIN / 2 ? live_bytes * 2 : THRESHOLD_MIN;
    collecting = false;
}

void gc_root_hook(void (*f)())
{
    for (usize i = 0; i < nhooks; i++)
        if (hooks[i] == f)
            return;
    // Dropping one would lose a whole subsystem's roots, and the wreckage
    // would turn up somewhere else entirely.
    if (nhooks >= MAX_HOOKS)
        panic("gc: too many root hooks");
    hooks[nhooks++] = f;
}

void gc_stress(bool on)
{
    stress = on;
}

GcStats gc_stats()
{
    return GcStats{ live_objects, live_bytes, collections, freed_total };
}

void gc_sweep_hook(void (*f)())
{
    for (usize i = 0; i < nsweepers; i++)
        if (sweepers[i] == f)
            return;
    if (nsweepers >= MAX_HOOKS)
        panic("gc: too many sweep hooks");
    sweepers[nsweepers++] = f;
}

bool gc_is_dying(Obj *o)
{
    return o && !(o->flags & (OBJ_MARK | OBJ_IMMORTAL));
}

bool gc_defer(Value obj, Value fn)
{
    if (!owed)
        owed = heap_new<Vec<Owed>>();
    if (!owed || !owed->push(Owed{ obj, fn }))
        return false;
    // Marked here rather than at the next collection, because this one has
    // already walked the roots.
    gc_mark(obj);
    gc_mark(fn);
    return true;
}

bool gc_owes()
{
    return owed && owed_at < owed->size();
}

bool gc_take(Value &obj, Value &fn)
{
    if (!gc_owes())
        return false;
    obj = (*owed)[owed_at].obj;
    fn  = (*owed)[owed_at].fn;
    owed_at++;
    // Emptied rather than grown for ever: the queue is short and bursty.
    if (owed_at == owed->size()) {
        owed->clear();
        owed_at = 0;
    }
    return true;
}

void gc_immortal(Obj *o)
{
    o->flags |= OBJ_IMMORTAL;
    o->next = all;
    o->grey = nullptr;
    all     = o;
    live_objects++;
}

Obj *obj_alloc(const Type *t, usize bytes)
{
    // Before the allocation, never during: what is being built is not yet
    // reachable from anything.
    if (!collecting && (stress || since_gc >= threshold))
        gc_collect();

    Obj *o = static_cast<Obj *>(heap_alloc(bytes));
    if (!o)
        return nullptr;

    o->type  = t;
    o->grey  = nullptr;
    o->flags = 0;
    o->next  = all;
    all      = o;

    usize got = heap_usable_size(o);
    live_objects++;
    live_bytes += got;
    since_gc += got;
    return o;
}
