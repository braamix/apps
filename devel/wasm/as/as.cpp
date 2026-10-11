#include "as.h"

#include "parser.h"
#include "writer.h"

bool assemble(Str name, Str source, Vec<u8> &out, Diag &diag)
{
    as::Asm a;
    if (!as::parse(name, source, a, diag)) {
        if (a.oom)
            diag.error("out of memory");
        return false;
    }
    return as::write_object(a, out, diag);
}
