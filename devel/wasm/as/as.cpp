#include "as.h"

#include "emit.h"

bool assemble(Str name, Str source, const AsConfig &c, Vec<u8> &out, Diag &diag)
{
    // Nothing is read yet: every source is the empty module.
    (void)name;
    (void)source;
    (void)c;
    Emit e{ out };
    e.u32le(0x6d736100); // \0asm
    e.u32le(1);          // version
    if (e.oom) {
        diag.error("out of memory");
        return false;
    }
    return true;
}
