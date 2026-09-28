#include "symbols.h"

#include "cursor.h"
#include "module.h"
#include "object.h"
#include "wasm.h"

using namespace wasm;

namespace {

// llvm-nm's letter: U or w undefined, W weak, t a function, d anything
// else; capital when the symbol is not local.
char type_of(u8 kind, bool undefined, bool weak, bool global)
{
    if (undefined)
        return weak ? 'w' : 'U';
    if (weak)
        return 'W';
    char t = kind == SYM_FUNCTION ? 't' : 'd';
    return global ? char(t - 'a' + 'A') : t;
}

ModuleSymbol make(Str name, u8 kind, u32 flags, u64 addr, u64 size)
{
    ModuleSymbol s{};
    s.name      = name;
    s.kind      = kind;
    s.undefined = flags & SYM_UNDEFINED;
    s.weak      = (flags & SYM_BINDING_MASK) == SYM_WEAK;
    s.global    = (flags & SYM_BINDING_MASK) != SYM_LOCAL;
    s.type      = type_of(kind, s.undefined, s.weak, s.global);
    if (!s.undefined) {
        s.addr = addr;
        s.size = size;
    }
    return s;
}

// A relocatable object, through read_object: every address counts from its
// section's start, as the sections of an object are at 0.
bool object_symbols(Str what, Bytes file, Vec<ModuleSymbol> &syms, Out &err)
{
    Object o;
    if (!o.name.assign(what)) {
        err.put(what).put(": out of memory");
        return false;
    }
    o.file = file;
    o.link = false;
    if (!read_object(o, err))
        return false;
    for (const Symbol &s : o.symbols) {
        u64 addr = 0, size = 0;
        if (!s.undefined()) {
            if (s.kind == SYM_FUNCTION) {
                const Function &f = o.functions[s.index - o.imported_functions];
                addr              = f.code_off;
                size              = f.body_off - f.code_off + f.body_size;
            } else if (s.kind == SYM_GLOBAL) {
                const Global &g = o.globals[s.index - o.imported_globals];
                addr            = g.off;
                size            = g.size;
            } else if (s.kind == SYM_DATA) {
                i64 base = 0;
                if (!(s.flags & SYM_ABSOLUTE)) {
                    const Segment &seg = o.segments[s.segment];
                    if (seg.flags != 1 && seg.offset.is_i32)
                        base = seg.offset.value;
                    else if (seg.flags != 1 && seg.offset.is_i64)
                        base = seg.offset.wide;
                }
                addr = u64(base) + s.offset;
                size = s.size;
            } else if (s.kind == SYM_TABLE || s.kind == SYM_TAG) {
                addr = s.index;
            }
        }
        ModuleSymbol n = make(s.name, s.kind, s.flags, addr, size);
        if (s.kind == SYM_DATA && !s.undefined() && !(s.flags & SYM_ABSOLUTE))
            n.segment = s.segment;
        if (!syms.push(n)) {
            err.put(what).put(": out of memory");
            return false;
        }
    }
    return true;
}

// ------------------------------------------------------------ programs

struct Func {
    u32 off;  // its size field, in the CODE section's contents
    u32 size; // size field and body
};

struct Glob {
    u32 off; // in the GLOBAL section's contents
    u32 size;
    i64 value; // a lone i32.const or i64.const, else 0
};

struct Seg {
    i64 base; // where it is placed; 0 for a passive one, or one at a global
    u32 size;
};

struct Exp {
    Str name;
    u8 kind;
    u32 index;
};

// What a linked module says of its functions, globals and data, as llvm
// reads it to make symbols of its names or exports.
struct Program {
    Program(Str w, Out &e) : what(w), err(e) {}

    Str what;
    Out &err;
    const Section *sec = nullptr;
    u32 funcs_imported = 0, globals_imported = 0;
    u32 funcs_declared = 0;
    Vec<Func> funcs;
    Vec<Glob> globals;
    Vec<Seg> segs;
    Vec<Exp> exports;
    const Section *names = nullptr;
    u32 code_at = 0, global_at = 0;
    bool placed = true; // false for a shared library: its sections are at 0

    bool fail(const Cursor &c)
    {
        err.put(what).put(": ").put(sec->name).put(" +0x").hex(u32(c.where())).put(": ");
        err.put(c.why());
        return false;
    }

    bool oom()
    {
        err.put(what).put(": out of memory");
        return false;
    }

    // Past a LEB128 of up to 64 bits.
    static void leb(Cursor &c) { c.sleb64(); }

    // A constant expression, through its end.
    static void expr(Cursor &c, i64 &value, u8 &first)
    {
        u32 ops = 0;
        value   = 0;
        first   = 0;
        for (;;) {
            usize at = c.at();
            u8 op    = c.byte();
            if (!c.ok() || op == OP_END)
                break;
            if (ops++ == 0)
                first = op;
            switch (op) {
            case OP_I32_CONST:
                value = c.sleb();
                break;
            case OP_I64_CONST:
                value = c.sleb64();
                break;
            case OP_F32_CONST:
                c.take(4);
                break;
            case OP_F64_CONST:
                c.take(8);
                break;
            case OP_GLOBAL_GET:
            case OP_REF_FUNC:
                c.uleb();
                break;
            case OP_REF_NULL:
                leb(c);
                break;
            case OP_I32_ADD:
            case OP_I32_SUB:
            case OP_I32_MUL:
            case OP_I64_ADD:
            case OP_I64_SUB:
            case OP_I64_MUL:
                break;
            case 0xfd: // v128.const
                if (c.uleb() == 12)
                    c.take(16);
                else
                    c.fail("not a constant instruction", at);
                break;
            default:
                c.fail("not a constant instruction", at);
                break;
            }
        }
        // More than one instruction is an extended expression, with no value.
        if (ops != 1 || (first != OP_I32_CONST && first != OP_I64_CONST))
            value = 0;
        if (ops != 1)
            first = 0;
    }

    static void limits(Cursor &c)
    {
        u8 flags = c.byte();
        leb(c);
        if (flags & LIMITS_MAX)
            leb(c);
    }

    bool imports(Cursor &c)
    {
        u32 n = c.count();
        for (u32 i = 0; i < n && c.ok(); i++) {
            c.name();
            c.name();
            usize at = c.at();
            switch (c.byte()) {
            case EXT_FUNCTION:
                c.uleb();
                funcs_imported++;
                break;
            case EXT_TABLE:
                leb(c);
                limits(c);
                break;
            case EXT_MEMORY:
                limits(c);
                break;
            case EXT_GLOBAL:
                leb(c);
                c.byte();
                globals_imported++;
                break;
            case EXT_TAG:
                c.byte();
                c.uleb();
                break;
            default:
                c.fail("unknown import kind", at);
                break;
            }
        }
        return c.ok();
    }

    bool global_section(Cursor &c)
    {
        u32 n = c.count();
        for (u32 i = 0; i < n && c.ok(); i++) {
            Glob g{};
            g.off = c.at();
            leb(c);
            c.byte();
            u8 first;
            expr(c, g.value, first);
            g.size = c.at() - g.off;
            if (c.ok() && !globals.push(g))
                return oom();
        }
        return c.ok();
    }

    bool export_section(Cursor &c)
    {
        u32 n = c.count();
        for (u32 i = 0; i < n && c.ok(); i++) {
            Exp e{};
            e.name   = c.name();
            usize at = c.at();
            e.kind   = c.byte();
            e.index  = c.uleb();
            if (c.ok() && e.kind > EXT_TAG)
                c.fail("unexpected export kind", at);
            if (c.ok() && e.kind == EXT_FUNCTION &&
                (e.index < funcs_imported || e.index >= funcs_imported + funcs_declared))
                c.fail("invalid function export", at);
            if (c.ok() && !exports.push(e))
                return oom();
        }
        return c.ok();
    }

    bool code_section(Cursor &c)
    {
        u32 n = c.count();
        for (u32 i = 0; i < n && c.ok(); i++) {
            Func f{};
            f.off = c.at();
            c.take(c.uleb());
            f.size = c.at() - f.off;
            if (c.ok() && !funcs.push(f))
                return oom();
        }
        return c.ok();
    }

    bool data_section(Cursor &c)
    {
        u32 n = c.count();
        for (u32 i = 0; i < n && c.ok(); i++) {
            usize at  = c.at();
            u32 flags = c.uleb();
            Seg s{};
            u8 first = 0;
            if (flags == 2)
                c.uleb();
            if (flags == 0 || flags == 2)
                expr(c, s.base, first);
            else if (flags != 1)
                c.fail("unknown data segment flags", at);
            s.size = c.take(c.uleb()).size();
            if (c.ok() && !segs.push(s))
                return oom();
        }
        return c.ok();
    }

    bool read(const Vec<Section> &sections)
    {
        for (const Section &s : sections) {
            if (s.id == SEC_CUSTOM && (s.name == "dylink" || s.name == "dylink.0"))
                placed = false;
            if (s.id == SEC_CUSTOM && s.name == "name")
                names = &s;
        }
        for (const Section &s : sections) {
            sec = &s;
            Cursor c(s.body);
            bool ok = true;
            switch (s.id) {
            case SEC_IMPORT:
                ok = imports(c);
                break;
            case SEC_FUNCTION:
                funcs_declared = c.count();
                break;
            case SEC_GLOBAL:
                global_at = s.start;
                ok        = global_section(c);
                break;
            case SEC_EXPORT:
                ok = export_section(c);
                break;
            case SEC_CODE:
                code_at = s.start;
                ok      = code_section(c);
                break;
            case SEC_DATA:
                ok = data_section(c);
                break;
            default:
                break;
            }
            if (!c.ok())
                return fail(c);
            if (!ok)
                return false;
        }
        if (!placed)
            code_at = global_at = 0;
        return true;
    }

    u64 func_addr(u32 index) const { return code_at + funcs[index - funcs_imported].off; }

    u64 func_size(u32 index) const { return funcs[index - funcs_imported].size; }

    bool defined_func(u32 index) const
    {
        return index >= funcs_imported && index - funcs_imported < funcs.size();
    }

    bool exported(u32 index) const
    {
        for (const Exp &e : exports)
            if (e.kind == EXT_FUNCTION && e.index == index)
                return true;
        return false;
    }

    // Symbols of the exports, as llvm makes them with no name section: a
    // global's is a data symbol at its value, counted from the first
    // segment, and a memory has none.
    bool from_exports(Vec<ModuleSymbol> &syms)
    {
        for (const Exp &e : exports) {
            ModuleSymbol s{};
            switch (e.kind) {
            case EXT_FUNCTION:
                s = make(e.name, SYM_FUNCTION, 0, func_addr(e.index), func_size(e.index));
                break;
            case EXT_GLOBAL: {
                i64 v = 0;
                if (e.index >= globals_imported && e.index - globals_imported < globals.size())
                    v = globals[e.index - globals_imported].value;
                i64 base = segs.empty() ? 0 : segs[0].base;
                s        = make(e.name, SYM_DATA, 0, u64(base + v), 0);
                break;
            }
            case EXT_MEMORY:
                continue;
            default:
                s = make(e.name, SYM_TABLE, 0, e.index, 0);
                break;
            }
            if (!syms.push(s))
                return oom();
        }
        return true;
    }

    // Symbols of the name section's function, global and data segment names.
    bool from_names(Vec<ModuleSymbol> &syms)
    {
        sec = names;
        Cursor c(names->body);
        while (c.ok() && !c.done()) {
            u8 type    = c.byte();
            u32 size   = c.uleb();
            usize base = c.at();
            Cursor sub(c.take(size), base);
            if (!c.ok() || (type != 1 && type != 7 && type != 9))
                continue;
            u32 n = sub.count();
            for (u32 i = 0; i < n && sub.ok(); i++) {
                usize at  = sub.at();
                u32 index = sub.uleb();
                Str name  = sub.name();
                if (!sub.ok())
                    break;
                ModuleSymbol s{};
                if (type == 1) {
                    if (index >= funcs_imported + funcs.size() || name.empty()) {
                        sub.fail("invalid function name entry", at);
                        break;
                    }
                    if (!defined_func(index))
                        s = make(name, SYM_FUNCTION, SYM_UNDEFINED, 0, 0);
                    else
                        s = make(name, SYM_FUNCTION, exported(index) ? 0 : SYM_LOCAL,
                                 func_addr(index), func_size(index));
                } else if (type == 7) {
                    if (index >= globals_imported + globals.size() || name.empty()) {
                        sub.fail("invalid global name entry", at);
                        break;
                    }
                    if (index < globals_imported) {
                        s = make(name, SYM_GLOBAL, SYM_UNDEFINED, 0, 0);
                    } else {
                        const Glob &g = globals[index - globals_imported];
                        s             = make(name, SYM_GLOBAL, 0, global_at + g.off, g.size);
                    }
                } else {
                    if (index >= segs.size()) {
                        sub.fail("invalid data segment name entry", at);
                        break;
                    }
                    s = make(name, SYM_DATA, SYM_LOCAL, u64(segs[index].base), segs[index].size);
                    s.segment = index;
                }
                if (!syms.push(s))
                    return oom();
            }
            if (!sub.ok())
                return fail(sub);
            if (!sub.done()) {
                sub.fail("name sub-section ended prematurely");
                return fail(sub);
            }
        }
        return c.ok() || fail(c);
    }
};

} // namespace

bool read_symbols(Str what, Bytes file, Vec<ModuleSymbol> &syms, Out &err)
{
    Vec<Section> sections;
    if (!read_module(what, file, sections, err))
        return false;
    if (is_object(sections))
        return object_symbols(what, file, syms, err);
    Program p(what, err);
    if (!p.read(sections))
        return false;
    if (p.names && p.placed)
        return p.from_names(syms);
    return p.from_exports(syms);
}
