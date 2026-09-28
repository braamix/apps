// Liveness, as lld's MarkLive: from the roots through the relocations of
// every live chunk to the chunks they name. A worklist, never recursion.
#pragma once

#include "symtab.h"

// Marks InputFile::live_* and Sym::live. Without --gc-sections, all is live.
bool mark_live(Linker &l);

// --print-gc-sections: what was dropped, in lld's words and order.
void print_gc_sections(const Linker &l, Out &out);
