#include "intern.h"

#include "gc.h"
#include "kernel/alloc.h"
#include "kernel/hash.h"
#include "kernel/vec.h"
#include "obj.h"

namespace {

// HashMap and Vec have destructors, so this cannot be a namespace-scope
// object. Built on first use and never torn down: the kernel drops the
// instance.
struct Interns {
    HashMap<Str, StrObj *> by_bytes;
    Vec<StrObj *> all; // what by_bytes holds, in insertion order, to trace
};

Interns *table;

Interns *interns()
{
    if (!table)
        table = heap_new<Interns>();
    return table;
}

} // namespace

StrObj *str_intern(Str s)
{
    Interns *t = interns();
    if (!t)
        return nullptr;
    if (StrObj **found = t->by_bytes.find(s))
        return *found;

    StrObj *o = str_new(s);
    if (!o)
        return nullptr;
    // The key views the object's own bytes, which the table now keeps alive.
    if (!t->all.push(o) || !t->by_bytes.insert(o->str(), o)) {
        if (t->all.size() && t->all.back() == o)
            t->all.pop();
        return nullptr;
    }
    return o;
}

void intern_mark()
{
    if (!table)
        return;
    for (usize i = 0; i < table->all.size(); i++)
        gc_mark(obj_value(table->all[i]));
}

usize intern_count()
{
    return table ? table->all.size() : 0;
}
