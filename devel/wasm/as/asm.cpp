#include "asm.h"

namespace as {

u32 Asm::symbol(Str name)
{
    if (const u32 *k = by_name.find(name))
        return *k;
    u32 k = u32(syms.size());
    Symbol s;
    s.name = name;
    // llvm's private prefix: never imported, and named in the symbol table
    // only when a relocation needs it.
    s.temp = name.starts_with(".L");
    if (!syms.push(move(s)) || !by_name.insert(name, k))
        oom = true;
    return k;
}

u32 Asm::temp()
{
    u32 k = u32(syms.size());
    Symbol s;
    s.temp = true;
    if (!syms.push(move(s)))
        oom = true;
    return k;
}

void Asm::enlist(u32 sym)
{
    if (syms[sym].known)
        return;
    syms[sym].known = true;
    if (!order.push(sym))
        oom = true;
}

Str Asm::keep(Str s)
{
    String t;
    if (!t.assign(s) || !strings.push(move(t))) {
        oom = true;
        return Str();
    }
    return strings.back().str();
}

} // namespace as
