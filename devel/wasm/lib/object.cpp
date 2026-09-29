#include "object.h"

#include "cursor.h"
#include "kernel/alloc.h"

using namespace wasm;

namespace {

bool is_valtype(u8 t)
{
    return t == I32 || t == I64 || t == F32 || t == F64 || t == V128 || t == FUNCREF ||
           t == EXTERNREF;
}

bool is_reftype(u8 t)
{
    return t == FUNCREF || t == EXTERNREF;
}

struct Reader {
    Object &o;
    Out &err;
    const Section *sec = nullptr;

    // Import numbers of each kind, in their index spaces.
    Vec<u32> func_imports;
    Vec<u32> global_imports;
    Vec<u32> table_imports;
    Vec<u32> tag_imports;

    // `linking` and its subsections, gathered before any is parsed.
    u32 linking = NONE;
    Bytes subs[9];
    usize sub_base[9] = {};

    bool fail(Str why, usize at)
    {
        err.put(o.name.str()).put(": ");
        if (sec)
            err.put(sec->name).put(" +0x").hex(u32(at)).put(": ");
        err.put(why);
        return false;
    }

    bool fail(const Cursor &c) { return fail(c.why(), c.where()); }

    // What ld does not link: refused for it, read for the other tools.
    bool refuse(Cursor &c, Str why, usize at)
    {
        if (o.link)
            c.fail(why, at);
        return o.link;
    }

    // A value type: a byte, or a reference type with its heap type. Only
    // ld's are known when linking.
    void valtype(Cursor &c, usize at)
    {
        u8 t = c.byte();
        if (!c.ok() || is_valtype(t))
            return;
        if (o.link)
            c.fail("unknown value type", at);
        else if (t == 0x63 || t == 0x64)
            c.sleb64();
        else if (t < 0x69 || t > 0x74)
            c.fail("unknown value type", at);
    }

    // A struct's or an array's field: a value type or a packed one, and
    // whether it is mutable.
    void field(Cursor &c, usize at)
    {
        u8 t = peek(c);
        if (t == 0x78 || t == 0x77)
            c.byte();
        else
            valtype(c, at);
        c.byte();
    }

    // A vector of value types, as its bytes in the section.
    Bytes values(Cursor &c, usize at)
    {
        u32 n       = c.count();
        usize first = c.at();
        for (u32 i = 0; i < n && c.ok(); i++)
            valtype(c, at);
        return sec->body.subspan(first, c.at() - first);
    }

    // The next byte of a cursor over the whole section, not taken.
    u8 peek(const Cursor &c) { return c.left() ? sec->body[c.at()] : 0; }

    bool fail(const Cursor &c, const Out &why) { return fail(why.str(), c.where()); }

    // Checks a cursor that should have finished its section exactly.
    bool end(Cursor &c)
    {
        if (c.ok() && !c.done())
            c.fail("junk after the last entry");
        return c.ok() || fail(c);
    }

    bool limits(Cursor &c, Limits &l)
    {
        usize at = c.at();
        l.flags  = c.byte();
        if ((l.flags & LIMITS_64) && refuse(c, "64-bit limits: wasm64 is not linked here", at))
            return false;
        // 0x08 is a custom page size, which follows the maximum.
        u8 known = o.link ? LIMITS_MAX | LIMITS_SHARED : LIMITS_MAX | LIMITS_SHARED | LIMITS_64 | 8;
        if (l.flags & ~known) {
            c.fail("unknown limits flags");
            return false;
        }
        l.min = u32(c.uleb64());
        l.max = l.flags & LIMITS_MAX ? u32(c.uleb64()) : NONE;
        if (l.flags & 8)
            c.uleb();
        return c.ok();
    }

    // A constant expression, `body` being the section the cursor reads.
    bool expr_in(Cursor &c, Bytes body, Expr &e)
    {
        usize start = c.at();
        u32 ops     = 0;
        u8 first    = 0;
        e.value     = 0;
        e.wide      = 0;
        for (;;) {
            usize at = c.at();
            u8 op    = c.byte();
            if (!c.ok())
                return false;
            if (op == OP_END)
                break;
            if (ops++ == 0)
                first = op;
            switch (op) {
            case OP_I32_CONST:
                e.value = c.sleb();
                break;
            case OP_I64_CONST:
                e.wide = c.sleb64();
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
                c.sleb64();
                break;
            case OP_I32_ADD:
            case OP_I32_SUB:
            case OP_I32_MUL:
            case OP_I64_ADD:
            case OP_I64_SUB:
            case OP_I64_MUL:
                break;
            case 0xfd: // v128.const
                if (c.uleb() != 12) {
                    c.fail("not a constant instruction", at);
                    return false;
                }
                c.take(16);
                break;
            case 0xfb: // GC's
                switch (c.uleb()) {
                case 0x00: // struct.new
                case 0x01: // struct.new_default
                case 0x06: // array.new
                case 0x07: // array.new_default
                    c.uleb();
                    break;
                case 0x08: // array.new_fixed
                    c.uleb();
                    c.uleb();
                    break;
                case 0x1a: // any.convert_extern
                case 0x1b: // extern.convert_any
                case 0x1c: // ref.i31
                    break;
                default:
                    c.fail("not a constant instruction", at);
                    return false;
                }
                break;
            default:
                c.fail("not a constant instruction", at);
                return false;
            }
        }
        e.code   = body.subspan(start, c.at() - start);
        e.is_i32 = ops == 1 && first == OP_I32_CONST;
        e.is_i64 = ops == 1 && first == OP_I64_CONST;
        return c.ok();
    }

    bool type_section(Cursor &c)
    {
        u32 n = c.count();
        if (!o.types.reserve(n))
            return fail("out of memory", c.at());
        for (u32 i = 0; i < n && c.ok(); i++) {
            usize at = c.at();
            if (!o.link) {
                gc_types(c, at);
                continue;
            }
            if (c.byte() != FUNC_TYPE) {
                c.fail("not a function type; GC types are not linked here", at);
                break;
            }
            FuncType t;
            t.params  = c.take(c.count());
            t.results = c.take(c.count());
            for (u8 v : t.params)
                if (!is_valtype(v))
                    c.fail("unknown value type", at);
            for (u8 v : t.results)
                if (!is_valtype(v))
                    c.fail("unknown value type", at);
            o.types.push(t);
        }
        return end(c);
    }

    // A recursion group, a subtype or a composite type: one or more types
    // of the index space. A function's parameters and results are kept.
    void gc_types(Cursor &c, usize at)
    {
        u8 form = c.byte();
        u32 n   = 1;
        if (c.ok() && form == 0x4e) {
            n = c.count();
            if (n)
                form = c.byte();
        }
        for (u32 i = 0; i < n && c.ok(); i++) {
            if (i)
                form = c.byte();
            if (form == 0x50 || form == 0x4f) {
                u32 k = c.count();
                for (u32 j = 0; j < k && c.ok(); j++)
                    c.uleb();
                form = c.byte();
            }
            FuncType t{};
            if (form == FUNC_TYPE) {
                t.params  = values(c, at);
                t.results = values(c, at);
            } else if (form == 0x5f) {
                u32 k = c.count();
                for (u32 j = 0; j < k && c.ok(); j++)
                    field(c, at);
            } else if (form == 0x5e) {
                field(c, at);
            } else if (c.ok()) {
                c.fail("unknown type form", at);
            }
            if (c.ok() && !o.types.push(t))
                c.fail("out of memory", at);
        }
    }

    bool import_section(Cursor &c)
    {
        u32 n = c.count();
        if (!o.imports.reserve(n))
            return fail("out of memory", c.at());
        for (u32 i = 0; i < n && c.ok(); i++) {
            usize at = c.at();
            Import im{};
            im.module = c.name();
            im.field  = c.name();
            im.kind   = c.byte();
            u32 index = o.imports.size();
            switch (im.kind) {
            case EXT_FUNCTION:
                im.type = c.uleb();
                if (c.ok() && im.type >= o.types.size())
                    c.fail("type index out of range", at);
                func_imports.push(index);
                o.imported_functions++;
                break;
            case EXT_TABLE:
                im.valtype = reftype(c, at);
                limits(c, im.limits);
                table_imports.push(index);
                o.imported_tables++;
                break;
            case EXT_MEMORY:
                limits(c, im.limits);
                if (c.ok() && (im.limits.flags & LIMITS_SHARED))
                    refuse(c, "shared memory is not linked here", at);
                o.imported_memories++;
                break;
            case EXT_GLOBAL:
                im.valtype = peek(c);
                valtype(c, at);
                im.mut = c.byte();
                global_imports.push(index);
                o.imported_globals++;
                break;
            case EXT_TAG:
                if (refuse(c, "exception tags are not linked here; Braam has no exceptions", at))
                    break;
                c.byte();
                im.type = c.uleb();
                tag_imports.push(index);
                o.imported_tags++;
                break;
            default:
                c.fail("unknown import kind", at);
                break;
            }
            o.imports.push(im);
        }
        return end(c);
    }

    bool tag_section(Cursor &c)
    {
        u32 n = c.count();
        for (u32 i = 0; i < n && c.ok(); i++) {
            usize at = c.at();
            c.byte();
            if (c.uleb() >= o.types.size() && c.ok())
                c.fail("type index out of range", at);
        }
        o.tags = n;
        return end(c);
    }

    // A table's reference type: its first byte is kept.
    u8 reftype(Cursor &c, usize at)
    {
        u8 t = peek(c);
        if (o.link) {
            c.byte();
            if (c.ok() && !is_reftype(t))
                c.fail("unknown reference type", at);
        } else {
            valtype(c, at);
        }
        return t;
    }

    bool function_section(Cursor &c)
    {
        u32 n = c.count();
        if (!o.functions.reserve(n))
            return fail("out of memory", c.at());
        for (u32 i = 0; i < n && c.ok(); i++) {
            usize at = c.at();
            Function f{};
            f.type = c.uleb();
            if (c.ok() && f.type >= o.types.size())
                c.fail("type index out of range", at);
            f.comdat = NONE;
            o.functions.push(f);
        }
        return end(c);
    }

    bool table_section(Cursor &c)
    {
        u32 n = c.count();
        for (u32 i = 0; i < n && c.ok(); i++) {
            usize at = c.at();
            Table t{};
            // 0x40 0x00: a table with an initial value after its limits.
            bool init = false;
            if (!o.link && c.left() >= 2 && peek(c) == 0x40 && sec->body[c.at() + 1] == 0) {
                c.take(2);
                init = true;
            }
            t.reftype = reftype(c, at);
            limits(c, t.limits);
            if (init) {
                Expr e{};
                expr_in(c, sec->body, e);
            }
            o.tables.push(t);
        }
        return end(c);
    }

    bool memory_section(Cursor &c)
    {
        u32 n = c.count();
        for (u32 i = 0; i < n && c.ok(); i++) {
            usize at = c.at();
            Limits l{};
            limits(c, l);
            if (c.ok() && (l.flags & LIMITS_SHARED))
                refuse(c, "shared memory is not linked here", at);
            o.memories.push(l);
        }
        return end(c);
    }

    bool global_section(Cursor &c, Bytes body)
    {
        u32 n = c.count();
        if (!o.globals.reserve(n))
            return fail("out of memory", c.at());
        for (u32 i = 0; i < n && c.ok(); i++) {
            Global g{};
            g.off     = c.at();
            g.valtype = peek(c);
            valtype(c, g.off);
            g.mut = c.byte();
            expr_in(c, body, g.init);
            g.size = c.at() - g.off;
            o.globals.push(g);
        }
        return end(c);
    }

    bool export_section(Cursor &c)
    {
        u32 n = c.count();
        for (u32 i = 0; i < n && c.ok(); i++) {
            Export e{};
            e.name  = c.name();
            e.kind  = c.byte();
            e.index = c.uleb();
            o.exports.push(e);
        }
        return end(c);
    }

    bool elem_section(Cursor &c, Bytes body)
    {
        u32 n = c.count();
        for (u32 i = 0; i < n && c.ok(); i++) {
            usize at  = c.at();
            u32 flags = c.uleb();
            if (flags > 7) {
                c.fail("unknown element segment flags", at);
                break;
            }
            Expr e{};
            if (!(flags & 1)) {
                if (flags & 2)
                    c.uleb();
                expr_in(c, body, e);
            }
            if ((flags & 3) && (flags & 4) && !o.link)
                valtype(c, at);
            else if (flags & 3)
                c.byte();
            u32 k = c.count();
            for (u32 j = 0; j < k && c.ok(); j++)
                if (flags & 4)
                    expr_in(c, body, e);
                else
                    c.uleb();
        }
        o.elem_segments = n;
        return end(c);
    }

    bool code_section(Cursor &c)
    {
        usize at = c.at();
        u32 n    = c.count();
        if (c.ok() && n != o.functions.size())
            c.fail("function count differs from the FUNCTION section's", at);
        for (u32 i = 0; i < n && c.ok(); i++) {
            Function &f = o.functions[i];
            f.code_off  = c.at();
            f.body_size = c.uleb();
            f.body_off  = c.at();
            c.take(f.body_size);
            if (c.ok() && f.body_size == 0)
                c.fail("empty function body", f.code_off);
        }
        return end(c);
    }

    bool data_section(Cursor &c, Bytes body)
    {
        usize head = c.at();
        u32 n      = c.count();
        if (c.ok() && o.data_count != NONE && n != o.data_count)
            c.fail("segment count differs from DATACOUNT", head);
        if (!o.segments.reserve(n))
            return fail("out of memory", c.at());
        for (u32 i = 0; i < n && c.ok(); i++) {
            Segment s{};
            usize at = c.at();
            s.flags  = c.uleb();
            if (s.flags > 2) {
                c.fail("unknown data segment flags", at);
                break;
            }
            if (s.flags == 2)
                c.uleb();
            if (s.flags != 1)
                expr_in(c, body, s.offset);
            u32 len       = c.uleb();
            s.content_off = c.at();
            s.content     = c.take(len);
            s.comdat      = NONE;
            o.segments.push(s);
        }
        return end(c);
    }

    // ------------------------------------------------------------ linking

    bool symbol_table(Cursor &c)
    {
        u32 n = c.count();
        if (!o.symbols.reserve(n))
            return fail("out of memory", c.where());
        for (u32 i = 0; i < n && c.ok(); i++) {
            usize at = c.at();
            Symbol s{};
            s.kind       = c.byte();
            s.flags      = c.uleb();
            s.index      = NONE;
            s.segment    = NONE;
            bool defined = !(s.flags & SYM_UNDEFINED);
            if ((s.flags & SYM_TLS) && refuse(c, "thread-local storage is not linked here", at))
                break;
            switch (s.kind) {
            case SYM_FUNCTION:
                element(c, s, at, o.imported_functions, o.total_functions(), func_imports);
                break;
            case SYM_GLOBAL:
                element(c, s, at, o.imported_globals, o.total_globals(), global_imports);
                break;
            case SYM_TABLE:
                element(c, s, at, o.imported_tables, o.total_tables(), table_imports);
                break;
            case SYM_DATA:
                s.name = c.name();
                if (defined) {
                    s.segment = c.uleb();
                    s.offset  = c.uleb();
                    s.size    = c.uleb();
                    if (c.ok() && !(s.flags & SYM_ABSOLUTE)) {
                        if (s.segment >= o.segments.size())
                            c.fail("data symbol names no segment", at);
                        else if (u64(s.offset) + s.size > o.segments[s.segment].content.size())
                            c.fail("data symbol runs past its segment", at);
                    }
                }
                break;
            case SYM_SECTION:
                s.index = c.uleb();
                if (c.ok() && s.index >= o.sections.size())
                    c.fail("section symbol names no section", at);
                else if (c.ok() && !(s.flags & SYM_LOCAL))
                    c.fail("section symbol is not local", at);
                else if (c.ok())
                    s.name = o.sections[s.index].name;
                break;
            case SYM_TAG:
                if (!refuse(c, "exception tags are not linked here; Braam has no exceptions", at))
                    element(c, s, at, o.imported_tags, o.total_tags(), tag_imports);
                break;
            default:
                c.fail("unknown symbol kind", at);
                break;
            }
            if (c.ok() && defined)
                name_element(s);
            o.symbols.push(s);
        }
        return c.ok();
    }

    // A defined function, global or table is named by its first symbol.
    void name_element(const Symbol &s)
    {
        Str *name = nullptr;
        if (s.kind == SYM_FUNCTION)
            name = &o.functions[s.index - o.imported_functions].name;
        else if (s.kind == SYM_GLOBAL)
            name = &o.globals[s.index - o.imported_globals].name;
        else if (s.kind == SYM_TABLE)
            name = &o.tables[s.index - o.imported_tables].name;
        if (name && name->empty())
            *name = s.name;
    }

    // A function, global or table symbol: an index, and a name unless an
    // undefined symbol takes its import's.
    void element(Cursor &c, Symbol &s, usize at, u32 imported, u32 total, const Vec<u32> &imports)
    {
        bool defined = !(s.flags & SYM_UNDEFINED);
        s.index      = c.uleb();
        if (!c.ok())
            return;
        if (s.index >= total) {
            c.fail("symbol index out of range", at);
            return;
        }
        if (defined && s.index < imported) {
            c.fail("defined symbol names an import", at);
            return;
        }
        if (!defined && s.index >= imported) {
            c.fail("undefined symbol names a definition", at);
            return;
        }
        if (defined || (s.flags & SYM_EXPLICIT_NAME))
            s.name = c.name();
        if (!defined) {
            s.import         = imports[s.index];
            const Import &im = o.imports[s.import];
            s.import_module  = im.module;
            s.import_field   = im.field;
            if (!(s.flags & SYM_EXPLICIT_NAME))
                s.name = im.field;
        }
    }

    bool segment_info(Cursor &c)
    {
        usize head = c.at();
        u32 n      = c.count();
        if (c.ok() && n != o.segments.size())
            c.fail("segment count differs from the DATA section's", head);
        for (u32 i = 0; i < n && c.ok(); i++) {
            Segment &s  = o.segments[i];
            usize at    = c.at();
            s.name      = c.name();
            s.align     = c.uleb();
            s.seg_flags = c.uleb();
            if (c.ok() && s.align >= 32)
                c.fail("segment alignment out of range", at);
            if (c.ok() && (s.seg_flags & SEG_TLS))
                refuse(c, "thread-local storage is not linked here", at);
        }
        return c.ok();
    }

    bool init_funcs(Cursor &c)
    {
        u32 n = c.count();
        for (u32 i = 0; i < n && c.ok(); i++) {
            usize at = c.at();
            InitFunc f{};
            f.priority = c.uleb();
            f.symbol   = c.uleb();
            if (c.ok() &&
                (f.symbol >= o.symbols.size() || o.symbols[f.symbol].kind != SYM_FUNCTION))
                c.fail("init function is not a function symbol", at);
            o.init_funcs.push(f);
        }
        return c.ok();
    }

    bool comdat_info(Cursor &c)
    {
        u32 n = c.count();
        for (u32 i = 0; i < n && c.ok(); i++) {
            usize at = c.at();
            Comdat cd;
            cd.name = c.name();
            if (c.uleb() != 0 && c.ok())
                c.fail("unknown comdat flags", at);
            u32 k = c.count();
            for (u32 j = 0; j < k && c.ok(); j++) {
                usize pos = c.at();
                ComdatEntry e{};
                e.kind  = c.byte();
                e.index = c.uleb();
                if (!c.ok())
                    break;
                u32 *owner = nullptr;
                switch (e.kind) {
                case COMDAT_DATA:
                    if (e.index >= o.segments.size())
                        c.fail("comdat names no segment", pos);
                    else
                        owner = &o.segments[e.index].comdat;
                    break;
                case COMDAT_FUNCTION:
                    if (e.index < o.imported_functions || e.index >= o.total_functions())
                        c.fail("comdat names no defined function", pos);
                    else
                        owner = &o.functions[e.index - o.imported_functions].comdat;
                    break;
                case COMDAT_SECTION:
                    if (e.index >= o.sections.size())
                        c.fail("comdat names no section", pos);
                    else
                        owner = &o.sections[e.index].comdat;
                    break;
                default:
                    c.fail("unknown comdat entry kind", pos);
                    break;
                }
                if (owner && *owner != NONE)
                    c.fail("in two comdats", pos);
                else if (owner)
                    *owner = o.comdats.size();
                cd.entries.push(e);
            }
            o.comdats.push(move(cd));
        }
        return c.ok();
    }

    bool linking_section(u32 index)
    {
        sec = &o.sections[index];
        Cursor c(sec->body);
        u32 version = c.uleb();
        if (c.ok() && version != LINKING_VERSION) {
            Out m;
            m.put("version ").num(version).put("; ld reads version 2");
            return fail(m.str(), 0);
        }
        while (c.ok() && !c.done()) {
            usize at = c.at();
            u8 type  = c.byte();
            u32 len  = c.uleb();
            Bytes b  = c.take(len);
            if (!c.ok())
                break;
            if (type < SUB_SEGMENT_INFO || type > SUB_SYMBOL_TABLE) {
                c.fail("unknown linking subsection", at);
                break;
            }
            if (subs[type].data()) {
                c.fail("repeated linking subsection", at);
                break;
            }
            subs[type]     = b.empty() ? Bytes(sec->body.data(), 0) : b;
            sub_base[type] = c.at() - len;
        }
        if (!c.ok())
            return fail(c);

        // The symbol table first: the others name symbols, and it names
        // segments, which segment info then describes.
        static const u8 ORDER[] = { SUB_SYMBOL_TABLE, SUB_SEGMENT_INFO, SUB_INIT_FUNCS,
                                    SUB_COMDAT_INFO };
        for (u8 type : ORDER) {
            if (!subs[type].data())
                continue;
            Cursor s(subs[type], sub_base[type]);
            bool ok = type == SUB_SYMBOL_TABLE   ? symbol_table(s)
                      : type == SUB_SEGMENT_INFO ? segment_info(s)
                      : type == SUB_INIT_FUNCS   ? init_funcs(s)
                                                 : comdat_info(s);
            if (ok && !s.done())
                s.fail("junk after the last entry");
            if (!s.ok())
                return fail(s);
            if (!ok)
                return false;
        }
        return true;
    }

    // ------------------------------------------------------------ relocations

    bool reloc_section(u32 index)
    {
        sec = &o.sections[index];
        Cursor c(sec->body);
        usize head = c.at();
        u32 target = c.uleb();
        if (!c.ok())
            return fail(c);
        if (target >= o.sections.size()) {
            c.fail("target section out of range", head);
            return fail(c);
        }
        // Not linking, relocations elsewhere are read and set aside, as
        // llvm reads them: wabt writes them for init expressions.
        const Section &t = o.sections[target];
        bool aside       = t.id != SEC_CODE && t.id != SEC_DATA && t.id != SEC_CUSTOM;
        if (aside && o.link) {
            c.fail("relocations for a section that takes none", head);
            return fail(c);
        }
        for (const RelocSection &r : o.relocs)
            if (r.target == target) {
                c.fail("second relocation section for one target", head);
                return fail(c);
            }

        RelocSection rs;
        rs.target = target;
        u32 n     = c.count();
        if (!rs.relocs.reserve(n))
            return fail("out of memory", c.at());
        u32 prev  = 0;
        u32 chunk = 0; // the function or segment the last one landed in
        for (u32 i = 0; i < n && c.ok(); i++) {
            usize at = c.at();
            Reloc r{};
            r.type = c.byte();
            if (!c.ok())
                break;
            if (r.type >= R_COUNT) {
                Out m;
                m.put("unknown relocation type ").num(r.type);
                return fail(m.str(), at);
            }
            Str refusal = o.link ? reloc_refusal(r.type) : Str();
            if (!refusal.empty()) {
                Out m;
                m.put(reloc_name(r.type)).put(": ").put(refusal);
                return fail(m.str(), at);
            }
            r.offset = c.uleb();
            r.index  = c.uleb();
            r.addend = reloc_has_addend(r.type) ? c.sleb() : 0;
            if (!c.ok())
                break;

            u8 kind = reloc_symbol_kind(r.type);
            if (kind == SYM_NONE) {
                if (r.index >= o.types.size()) {
                    Out m;
                    m.put("type ").num(r.index).put(" out of range");
                    return fail(m.str(), at);
                }
            } else if (r.index >= o.symbols.size()) {
                Out m;
                m.put("symbol ").num(r.index).put(" out of range");
                return fail(m.str(), at);
            } else if (o.symbols[r.index].kind != kind) {
                Out m;
                m.put(reloc_name(r.type)).put(" names the wrong kind of symbol, ");
                m.put(o.symbols[r.index].name);
                return fail(m.str(), at);
            }

            if (r.offset < prev)
                return fail("relocations not in offset order", at);
            prev      = r.offset;
            u32 width = reloc_width(r.type);
            if (u64(r.offset) + width > t.body.size())
                return fail("relocation past the end of its section", at);

            if (t.id == SEC_CODE) {
                while (chunk < o.functions.size() &&
                       o.functions[chunk].body_off + o.functions[chunk].body_size <= r.offset)
                    chunk++;
                if (chunk == o.functions.size() || r.offset < o.functions[chunk].body_off ||
                    r.offset + width > o.functions[chunk].body_off + o.functions[chunk].body_size)
                    return fail("relocation outside every function body", at);
                r.chunk = chunk;
                r.at    = r.offset - o.functions[chunk].body_off;
            } else if (t.id == SEC_DATA) {
                while (chunk < o.segments.size() &&
                       o.segments[chunk].content_off + o.segments[chunk].content.size() <= r.offset)
                    chunk++;
                if (chunk == o.segments.size() || r.offset < o.segments[chunk].content_off ||
                    r.offset + width >
                        o.segments[chunk].content_off + o.segments[chunk].content.size()) {
                    if (!o.link)
                        continue;
                    return fail("relocation outside every data segment", at);
                }
                r.chunk = chunk;
                r.at    = r.offset - o.segments[chunk].content_off;
            } else {
                r.chunk = 0;
                r.at    = r.offset;
            }
            rs.relocs.push(r);
        }
        if (!end(c))
            return false;
        if (!aside)
            o.relocs.push(move(rs));
        return true;
    }

    // ------------------------------------------------------------ custom

    bool features(Cursor &c)
    {
        u32 n = c.count();
        for (u32 i = 0; i < n && c.ok(); i++) {
            usize at = c.at();
            Feature f{};
            f.prefix = c.byte();
            f.name   = c.name();
            if (c.ok() && f.prefix != '+' && f.prefix != '-' && f.prefix != '=')
                c.fail("unknown feature prefix", at);
            o.features.push(f);
        }
        return end(c);
    }

    bool producers(Cursor &c)
    {
        u32 n = c.count();
        for (u32 i = 0; i < n && c.ok(); i++) {
            Str field = c.name();
            u32 k     = c.count();
            for (u32 j = 0; j < k && c.ok(); j++) {
                Producer p{};
                p.field   = field;
                p.name    = c.name();
                p.version = c.name();
                o.producers.push(p);
            }
        }
        return end(c);
    }

    // ------------------------------------------------------------ the file

    bool read()
    {
        // What ld does not link, in its own words, before the framing.
        Bytes f = o.file;
        if (f.size() >= 4 && f[0] == 'B' && f[1] == 'C' && f[2] == 0xc0 && f[3] == 0xde)
            return fail("LLVM bitcode (from -flto); ld links wasm objects only", 0);
        if (f.size() < 8 || f[0] != MAGIC[0] || f[1] != MAGIC[1] || f[2] != MAGIC[2] ||
            f[3] != MAGIC[3])
            return fail("not a wasm object", 0);
        if (!read_module(o.name.str(), f, o.sections, err))
            return false;

        for (u32 i = 0; i < o.sections.size(); i++) {
            sec = &o.sections[i];
            Cursor c(sec->body);
            bool ok = true;
            switch (sec->id) {
            case SEC_TYPE:
                ok = type_section(c);
                break;
            case SEC_IMPORT:
                ok = import_section(c);
                break;
            case SEC_FUNCTION:
                ok = function_section(c);
                break;
            case SEC_TABLE:
                o.table_section = i;
                ok              = table_section(c);
                break;
            case SEC_MEMORY:
                ok = memory_section(c);
                break;
            case SEC_GLOBAL:
                o.global_section = i;
                ok               = global_section(c, sec->body);
                break;
            case SEC_EXPORT:
                ok = export_section(c);
                break;
            case SEC_START:
                o.start = c.uleb();
                ok      = end(c);
                break;
            case SEC_ELEM:
                ok = elem_section(c, sec->body);
                break;
            case SEC_DATACOUNT:
                o.data_count = c.uleb();
                ok           = end(c);
                break;
            case SEC_CODE:
                o.code_section = i;
                ok             = code_section(c);
                break;
            case SEC_DATA:
                o.data_section = i;
                ok             = data_section(c, sec->body);
                break;
            case SEC_TAG:
                if (o.link)
                    return fail("exception tags are not linked here; Braam has no exceptions", 0);
                ok = tag_section(c);
                break;
            default:
                if (sec->name == "linking") {
                    if (linking != NONE)
                        return fail("second linking section", 0);
                    linking = i;
                } else if (sec->name == "target_features") {
                    ok = features(c);
                } else if (sec->name == "producers") {
                    ok = producers(c);
                }
                break;
            }
            if (!ok)
                return false;
        }
        sec = nullptr;

        if (o.code_section == NONE && !o.functions.empty())
            return fail("functions declared but no CODE section", 0);
        if (o.data_section == NONE && o.data_count != NONE && o.data_count != 0)
            return fail("DATACOUNT but no DATA section", 0);
        // Not linking, a program is read for its sections alone.
        if (linking == NONE && !o.link)
            return true;
        if (linking == NONE)
            return fail("no linking section, so not a relocatable object (clang -c makes one)", 0);
        if (!linking_section(linking))
            return false;
        for (u32 i = 0; i < o.sections.size(); i++)
            if (o.sections[i].id == SEC_CUSTOM && o.sections[i].name.starts_with("reloc."))
                if (!reloc_section(i))
                    return false;
        return true;
    }
};

} // namespace

bool read_object(Object &o, Out &err)
{
    Reader *r = heap_new<Reader>(o, err);
    if (!r) {
        err.put(o.name.str()).put(": out of memory");
        return false;
    }
    bool ok = r->read();
    heap_delete(r);
    return ok;
}
