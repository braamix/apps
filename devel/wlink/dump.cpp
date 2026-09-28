#include "dump.h"

#include "kernel/alloc.h"
#include "reader.h"

using namespace wasm;

namespace {

// llvm-objdump's address for a symbol: a body's offset in CODE, a datum's
// address in the object's own memory, a global's offset in GLOBAL, or an
// index.
u32 address(const Object &o, const Symbol &s)
{
    if (s.undefined())
        return 0;
    switch (s.kind) {
    case SYM_FUNCTION:
        return o.functions[s.index - o.imported_functions].code_off;
    case SYM_GLOBAL:
        return o.globals[s.index - o.imported_globals].off;
    case SYM_DATA: {
        if (s.flags & SYM_ABSOLUTE)
            return s.offset;
        const Segment &seg = o.segments[s.segment];
        return (seg.offset.is_i32 ? u32(seg.offset.value) : 0) + s.offset;
    }
    case SYM_TABLE:
        return s.index;
    default:
        return 0;
    }
}

u32 size(const Object &o, const Symbol &s)
{
    if (s.undefined())
        return 0;
    switch (s.kind) {
    case SYM_FUNCTION: {
        const Function &f = o.functions[s.index - o.imported_functions];
        return f.body_off - f.code_off + f.body_size;
    }
    case SYM_GLOBAL:
        return o.globals[s.index - o.imported_globals].size;
    case SYM_DATA:
        return s.size;
    default:
        return 0;
    }
}

Str section_of(const Object &o, const Symbol &s)
{
    if (s.undefined())
        return "*UND*";
    switch (s.kind) {
    case SYM_FUNCTION:
        return "CODE";
    case SYM_DATA:
        return "DATA";
    case SYM_GLOBAL:
        return "GLOBAL";
    case SYM_TABLE:
        return "TABLE";
    case SYM_SECTION:
        return o.sections[s.index].name;
    default:
        return "TAG";
    }
}

} // namespace

void dump_object(const Object &o, Out &out)
{
    out.put('\n').put(o.name.str()).put(":\tfile format wasm\n\n");

    out.put("SYMBOL TABLE:\n");
    for (const Symbol &s : o.symbols) {
        char bind = s.undefined() || s.weak() ? ' ' : s.local() ? 'l' : 'g';
        char type = s.kind == SYM_FUNCTION ? 'F' : s.kind == SYM_DATA ? 'O' : ' ';
        out.hex(address(o, s), 8).put(' ').put(bind).put(s.weak() ? 'w' : ' ').put("   ");
        out.put(s.kind == SYM_SECTION ? 'd' : ' ').put(type).put(' ');
        out.put(section_of(o, s)).put('\t').hex(size(o, s), 8);
        if (s.flags & SYM_HIDDEN)
            out.put(" .hidden");
        out.put(' ').put(s.name).put('\n');
    }

    // In the order of the sections they patch.
    for (u32 i = 0; i < o.sections.size(); i++)
        for (const RelocSection &rs : o.relocs) {
            if (rs.target != i || rs.relocs.empty())
                continue;
            out.put("\nRELOCATION RECORDS FOR [").put(o.sections[i].name).put("]:\n");
            out.put("OFFSET   TYPE                     VALUE\n");
            for (const Reloc &r : rs.relocs) {
                out.hex(r.offset, 8).put(' ').left(reloc_name(r.type), 24).put(' ');
                if (reloc_symbol_kind(r.type) == SYM_NONE)
                    out.num(r.index);
                else
                    out.put(o.symbols[r.index].name);
                if (r.addend >= 0)
                    out.put('+');
                out.snum(r.addend).put('\n');
            }
        }
}

namespace {

bool dump_one(Str name, Str member, Bytes bytes, Out &out, Out &err)
{
    Object *o = heap_new<Object>();
    if (!o) {
        err.put(name).put(": out of memory");
        return false;
    }
    o->file = bytes;
    o->name.append(name);
    if (!member.empty()) {
        o->name.push('(');
        o->name.append(member);
        o->name.push(')');
    }
    bool ok = read_object(*o, err);
    if (ok)
        dump_object(*o, out);
    heap_delete(o);
    return ok;
}

} // namespace

bool dump_file(Str name, Bytes file, Out &out, Out &err)
{
    if (!is_archive(file))
        return dump_one(name, Str(), file, out, err);
    Vec<Member> members;
    if (!read_archive(name, file, members, err))
        return false;
    for (const Member &m : members)
        if (!dump_one(name, m.name, m.data, out, err))
            return false;
    return true;
}
