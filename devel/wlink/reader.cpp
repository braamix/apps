#include "reader.h"

using namespace wasm;

// ---------------------------------------------------------------- cursor

void Cursor::fail(Str why, usize at)
{
    if (!failed_) {
        failed_ = true;
        why_    = why;
        where_  = at;
    }
    at_ = b_.size();
}

u8 Cursor::byte()
{
    if (at_ >= b_.size()) {
        fail("unexpected end of section");
        return 0;
    }
    return b_[at_++];
}

u32 Cursor::u32le()
{
    if (left() < 4) {
        fail("unexpected end of section");
        return 0;
    }
    u32 v = u32(b_[at_]) | u32(b_[at_ + 1]) << 8 | u32(b_[at_ + 2]) << 16 | u32(b_[at_ + 3]) << 24;
    at_ += 4;
    return v;
}

u32 Cursor::uleb()
{
    usize start = at_;
    u32 v       = 0;
    for (u32 i = 0; i < 5; i++) {
        if (at_ >= b_.size()) {
            fail("unexpected end of section", start);
            return 0;
        }
        u8 x = b_[at_++];
        v |= u32(x & 0x7f) << (7 * i);
        if (!(x & 0x80)) {
            if (i == 4 && (x & 0x70)) {
                fail("LEB does not fit 32 bits", start);
                return 0;
            }
            return v;
        }
    }
    fail("LEB longer than 5 bytes", start);
    return 0;
}

i32 Cursor::sleb()
{
    usize start = at_;
    u32 v       = 0;
    for (u32 i = 0; i < 5; i++) {
        if (at_ >= b_.size()) {
            fail("unexpected end of section", start);
            return 0;
        }
        u8 x = b_[at_++];
        v |= u32(x & 0x7f) << (7 * i);
        if (!(x & 0x80)) {
            if (i == 4) {
                u8 top = x & 0x78; // bit 3 is the sign; 4-6 must repeat it
                if (top != 0 && top != 0x78) {
                    fail("LEB does not fit 32 bits", start);
                    return 0;
                }
            } else if (x & 0x40) {
                v |= ~u32(0) << (7 * (i + 1));
            }
            return i32(v);
        }
    }
    fail("LEB longer than 5 bytes", start);
    return 0;
}

i64 Cursor::sleb64()
{
    usize start = at_;
    u64 v       = 0;
    for (u32 i = 0; i < 10; i++) {
        if (at_ >= b_.size()) {
            fail("unexpected end of section", start);
            return 0;
        }
        u8 x = b_[at_++];
        v |= u64(x & 0x7f) << (7 * i);
        if (!(x & 0x80)) {
            if (i == 9) {
                if (x != 0 && x != 0x7f) {
                    fail("LEB does not fit 64 bits", start);
                    return 0;
                }
            } else if (x & 0x40) {
                v |= ~u64(0) << (7 * (i + 1));
            }
            return i64(v);
        }
    }
    fail("LEB longer than 10 bytes", start);
    return 0;
}

Bytes Cursor::take(usize n)
{
    if (n > left()) {
        fail("unexpected end of section");
        return Bytes();
    }
    Bytes b = b_.subspan(at_, n);
    at_ += n;
    return b;
}

Str Cursor::name()
{
    u32 n   = uleb();
    Bytes b = take(n);
    return Str(reinterpret_cast<const char *>(b.data()), b.size());
}

u32 Cursor::count()
{
    usize start = at_;
    u32 n       = uleb();
    if (n > left()) {
        fail("count is larger than the section", start);
        return 0;
    }
    return n;
}

// ---------------------------------------------------------------- objects

namespace {

// Where a standard section may stand. TAG sits between MEMORY and GLOBAL,
// DATACOUNT between ELEM and CODE.
u32 rank(u8 id)
{
    static const u8 RANK[] = { 0, 1, 2, 3, 4, 5, 7, 8, 9, 10, 12, 13, 11, 6 };
    return RANK[id];
}

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
        l.flags = c.byte();
        if (l.flags & LIMITS_64) {
            c.fail("64-bit limits: wasm64 is not linked here");
            return false;
        }
        if (l.flags & ~(LIMITS_MAX | LIMITS_SHARED)) {
            c.fail("unknown limits flags");
            return false;
        }
        l.min = c.uleb();
        l.max = l.flags & LIMITS_MAX ? c.uleb() : NONE;
        return c.ok();
    }

    // A constant expression, `body` being the section the cursor reads.
    bool expr_in(Cursor &c, Bytes body, Expr &e)
    {
        usize start = c.at();
        u32 ops     = 0;
        u8 first    = 0;
        e.value     = 0;
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
                c.sleb64();
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
                c.byte();
                break;
            case OP_I32_ADD:
            case OP_I32_SUB:
            case OP_I32_MUL:
            case OP_I64_ADD:
            case OP_I64_SUB:
            case OP_I64_MUL:
                break;
            default:
                c.fail("not a constant instruction", at);
                return false;
            }
        }
        e.code   = body.subspan(start, c.at() - start);
        e.is_i32 = ops == 1 && first == OP_I32_CONST;
        return c.ok();
    }

    bool type_section(Cursor &c)
    {
        u32 n = c.count();
        if (!o.types.reserve(n))
            return fail("out of memory", c.at());
        for (u32 i = 0; i < n && c.ok(); i++) {
            usize at = c.at();
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
                im.valtype = c.byte();
                if (c.ok() && !is_reftype(im.valtype))
                    c.fail("unknown reference type", at);
                limits(c, im.limits);
                table_imports.push(index);
                o.imported_tables++;
                break;
            case EXT_MEMORY:
                limits(c, im.limits);
                if (c.ok() && (im.limits.flags & LIMITS_SHARED))
                    c.fail("shared memory is not linked here", at);
                o.imported_memories++;
                break;
            case EXT_GLOBAL:
                im.valtype = c.byte();
                im.mut     = c.byte();
                if (c.ok() && !is_valtype(im.valtype))
                    c.fail("unknown value type", at);
                global_imports.push(index);
                o.imported_globals++;
                break;
            case EXT_TAG:
                c.fail("exception tags are not linked here; Braam has no exceptions", at);
                break;
            default:
                c.fail("unknown import kind", at);
                break;
            }
            o.imports.push(im);
        }
        return end(c);
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
            t.reftype = c.byte();
            if (c.ok() && !is_reftype(t.reftype))
                c.fail("unknown reference type", at);
            limits(c, t.limits);
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
                c.fail("shared memory is not linked here", at);
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
            g.valtype = c.byte();
            g.mut     = c.byte();
            if (c.ok() && !is_valtype(g.valtype))
                c.fail("unknown value type", g.off);
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
            if (flags & 3)
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
            if (s.flags & SYM_TLS) {
                c.fail("thread-local storage is not linked here", at);
                break;
            }
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
                c.fail("exception tags are not linked here; Braam has no exceptions", at);
                break;
            default:
                c.fail("unknown symbol kind", at);
                break;
            }
            o.symbols.push(s);
        }
        return c.ok();
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
                c.fail("thread-local storage is not linked here", at);
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
            m.put("version ").num(version).put("; wlink reads version 2");
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
        const Section &t = o.sections[target];
        if (t.id != SEC_CODE && t.id != SEC_DATA && t.id != SEC_CUSTOM) {
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
            Str refusal = reloc_refusal(r.type);
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
                        o.segments[chunk].content_off + o.segments[chunk].content.size())
                    return fail("relocation outside every data segment", at);
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

    bool sections()
    {
        Cursor c(o.file);
        c.take(8);
        u32 last = 0;
        while (c.ok() && !c.done()) {
            usize at = c.at();
            Section s{};
            s.id     = c.byte();
            u32 size = c.uleb();
            if (c.ok() && size > c.left())
                c.fail("runs past the end of the file", at);
            Bytes b = c.take(size);
            if (!c.ok())
                break;
            if (s.id > SEC_TAG) {
                c.fail("unknown section id", at);
                break;
            }
            if (s.id == SEC_CUSTOM) {
                Cursor n(b);
                s.name = n.name();
                if (!n.ok()) {
                    c.fail("custom section name runs past the section", at);
                    break;
                }
                s.body     = b.subspan(n.at());
                s.file_off = b.data() - o.file.data() + n.at();
            } else {
                if (rank(s.id) <= last) {
                    c.fail("standard section out of order or repeated", at);
                    break;
                }
                last       = rank(s.id);
                s.name     = section_name(s.id);
                s.body     = b;
                s.file_off = b.data() - o.file.data();
            }
            if (!o.sections.push(s))
                return fail("out of memory", at);
        }
        if (!c.ok()) {
            Out m;
            m.put("section at file offset 0x").hex(u32(c.where())).put(": ").put(c.why());
            return fail(m.str(), 0);
        }
        return true;
    }

    bool read()
    {
        Bytes f = o.file;
        if (f.size() >= 4 && f[0] == 'B' && f[1] == 'C' && f[2] == 0xc0 && f[3] == 0xde)
            return fail("LLVM bitcode (from -flto); wlink links wasm objects only", 0);
        if (f.size() < 8 || f[0] != MAGIC[0] || f[1] != MAGIC[1] || f[2] != MAGIC[2] ||
            f[3] != MAGIC[3])
            return fail("not a wasm object", 0);
        if (f[4] != VERSION || f[5] || f[6] || f[7])
            return fail("wasm version other than 1", 0);
        if (!sections())
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
                return fail("exception tags are not linked here; Braam has no exceptions", 0);
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

// ---------------------------------------------------------------- archives

bool is_archive(Bytes f)
{
    static const char MAGIC_AR[] = "!<arch>\n";
    if (f.size() < 8)
        return false;
    for (u32 i = 0; i < 8; i++)
        if (f[i] != u8(MAGIC_AR[i]))
            return false;
    return true;
}

namespace {

// A decimal field of an ar header, space-padded. NONE if malformed.
u32 decimal(Str s)
{
    while (!s.empty() && s[s.size() - 1] == ' ')
        s = s.substr(0, s.size() - 1);
    if (s.empty() || s.size() > 9)
        return NONE;
    u32 v = 0;
    for (char c : s) {
        if (c < '0' || c > '9')
            return NONE;
        v = v * 10 + u32(c - '0');
    }
    return v;
}

bool ar_fail(Out &err, Str name, usize at, Str why)
{
    err.put(name).put(": member at 0x").hex(u32(at)).put(": ").put(why);
    return false;
}

} // namespace

bool read_archive(Str name, Bytes f, Vec<Member> &members, Out &err)
{
    const char *text = reinterpret_cast<const char *>(f.data());
    if (f.size() >= 8 && Str(text, 8) == "!<thin>\n") {
        err.put(name).put(": thin archives are not linked here");
        return false;
    }
    if (!is_archive(f)) {
        err.put(name).put(": not an archive");
        return false;
    }
    Str longnames;
    usize at = 8;
    while (at < f.size()) {
        if (f.size() - at < 60)
            return ar_fail(err, name, at, "truncated header");
        Str hdr(text + at, 60);
        if (hdr[58] != '`' || hdr[59] != '\n')
            return ar_fail(err, name, at, "bad header terminator");
        u32 size = decimal(hdr.substr(48, 10));
        if (size == NONE)
            return ar_fail(err, name, at, "bad member size");
        usize data = at + 60;
        if (size > f.size() - data)
            return ar_fail(err, name, at, "member runs past the end of the archive");

        Str raw = hdr.substr(0, 16);
        while (!raw.empty() && raw[raw.size() - 1] == ' ')
            raw = raw.substr(0, raw.size() - 1);
        Bytes body = f.subspan(data, size);
        Str mname;
        bool skip = false;
        if (raw == "/" || raw == "/SYM64/") {
            skip = true;
        } else if (raw == "//") {
            longnames = Str(text + data, size);
            skip      = true;
        } else if (raw.starts_with("#1/")) {
            u32 n = decimal(raw.substr(3));
            if (n == NONE || n > size)
                return ar_fail(err, name, at, "bad BSD name length");
            mname = Str(text + data, n);
            while (!mname.empty() && mname[mname.size() - 1] == '\0')
                mname = mname.substr(0, mname.size() - 1);
            body = body.subspan(n);
        } else if (raw.size() > 1 && raw[0] == '/' && raw[1] >= '0' && raw[1] <= '9') {
            u32 off = decimal(raw.substr(1));
            if (off == NONE || off >= longnames.size())
                return ar_fail(err, name, at, "long name out of range");
            Str rest = longnames.substr(off);
            usize nl = rest.find('\n');
            mname    = nl == Str::npos ? rest : rest.substr(0, nl);
            if (mname.ends_with("/"))
                mname = mname.substr(0, mname.size() - 1);
        } else {
            mname = raw;
            if (mname.ends_with("/"))
                mname = mname.substr(0, mname.size() - 1);
        }
        if (mname.starts_with("__.SYMDEF"))
            skip = true;
        if (!skip && !members.push(Member{ mname, body, u32(data + (size - body.size())) })) {
            err.put(name).put(": out of memory");
            return false;
        }
        at = data + size + (size & 1);
    }
    return true;
}
