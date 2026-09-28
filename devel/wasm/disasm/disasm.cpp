#include "disasm.h"

#include "archive.h"
#include "cursor.h"
#include "demangle/demangle.h"
#include "kernel/alloc.h"
#include "module.h"
#include "object.h"
#include "wasm.h"

using namespace wasm;

namespace {

bool is_wasm(Bytes f)
{
    return f.size() >= 4 && f[0] == MAGIC[0] && f[1] == MAGIC[1] && f[2] == MAGIC[2] &&
           f[3] == MAGIC[3];
}

bool is_bitcode(Bytes f)
{
    return f.size() >= 4 && f[0] == 'B' && f[1] == 'C' && f[2] == 0xc0 && f[3] == 0xde;
}

// Byte order, as StringRef compares.
int compare(Str a, Str b)
{
    usize n = a.size() < b.size() ? a.size() : b.size();
    for (usize i = 0; i < n; i++)
        if (a[i] != b[i])
            return u8(a[i]) < u8(b[i]) ? -1 : 1;
    return a.size() < b.size() ? -1 : a.size() > b.size() ? 1 : 0;
}

// llvm-objdump's order of the symbols in a section: by address, then name.
bool before(const NmSymbol *a, const NmSymbol *b)
{
    if (a->addr != b->addr)
        return a->addr < b->addr;
    return compare(a->name, b->name) < 0;
}

// A bottom-up merge sort: stable, and no recursion.
template <class T, class Less>
bool sort(Vec<T> &v, Less less)
{
    if (v.size() < 2)
        return true;
    Vec<T> t;
    if (!t.resize(v.size()))
        return false;
    Vec<T> *a = &v, *b = &t;
    for (usize w = 1; w < v.size(); w *= 2) {
        for (usize lo = 0; lo < v.size(); lo += 2 * w) {
            usize mid = lo + w < v.size() ? lo + w : v.size();
            usize hi  = lo + 2 * w < v.size() ? lo + 2 * w : v.size();
            usize i = lo, j = mid, o = lo;
            while (i < mid && j < hi)
                (*b)[o++] = less((*a)[j], (*a)[i]) ? (*a)[j++] : (*a)[i++];
            while (i < mid)
                (*b)[o++] = (*a)[i++];
            while (j < hi)
                (*b)[o++] = (*a)[j++];
        }
        Vec<T> *x = a;
        a         = b;
        b         = x;
    }
    if (a != &v)
        for (usize i = 0; i < v.size(); i++)
            v[i] = t[i];
    return true;
}

// Past a constant expression; its value when it is a lone i32.const or
// i64.const.
void expr(Cursor &c, u64 &value)
{
    u32 ops    = 0;
    bool known = false;
    value      = 0;
    for (;;) {
        u8 op = c.byte();
        if (!c.ok() || op == OP_END)
            break;
        known = ops++ == 0 && (op == OP_I32_CONST || op == OP_I64_CONST);
        switch (op) {
        case OP_I32_CONST:
            value = u64(u32(c.sleb()));
            break;
        case OP_I64_CONST:
            value = u64(c.sleb64());
            break;
        case OP_GLOBAL_GET:
            c.uleb();
            break;
        case OP_I32_ADD:
        case OP_I32_SUB:
        case OP_I32_MUL:
        case OP_I64_ADD:
        case OP_I64_SUB:
        case OP_I64_MUL:
            break;
        default:
            c.fail("not a constant instruction");
            break;
        }
    }
    if (ops != 1 || !known)
        value = 0;
}

// The DATA section's segments.
bool read_data(const Section &s, Vec<DataSeg> &segs, Out &err, Str what)
{
    Cursor c(s.body);
    u32 n = c.count();
    for (u32 i = 0; i < n && c.ok(); i++) {
        DataSeg g{};
        u32 flags = c.uleb();
        if (flags == 2)
            c.uleb();
        if (flags == 0 || flags == 2)
            expr(c, g.base);
        else if (flags != 1)
            c.fail("unknown data segment flags");
        u32 size      = c.uleb();
        g.content_off = c.at();
        g.content     = c.take(size);
        if (c.ok() && !segs.push(g)) {
            err.put(what).put(": out of memory");
            return false;
        }
    }
    if (!c.ok()) {
        err.put(what).put(": DATA +0x").hex(u32(c.where())).put(": ").put(c.why());
        return false;
    }
    return true;
}

// Limits, and whether they are a 64-bit memory's.
bool limits64(Cursor &c)
{
    u8 flags = c.byte();
    c.uleb64();
    if (flags & LIMITS_MAX)
        c.uleb64();
    if (flags & 8)
        c.uleb();
    return flags & LIMITS_64;
}

void valtype(Cursor &c)
{
    u8 t = c.byte();
    if (t == 0x63 || t == 0x64)
        c.sleb64();
}

// Whether the module has a 64-bit memory, which llvm prints addresses of
// sixteen digits for.
bool memory64(const Vec<Section> &sections)
{
    bool wide = false;
    for (const Section &s : sections) {
        Cursor c(s.body);
        if (s.id == SEC_MEMORY) {
            for (u32 n = c.count(); n-- && c.ok();)
                wide |= limits64(c);
        } else if (s.id == SEC_IMPORT) {
            for (u32 n = c.count(); n-- && c.ok();) {
                c.name();
                c.name();
                switch (c.byte()) {
                case EXT_FUNCTION:
                    c.uleb();
                    break;
                case EXT_TABLE:
                    valtype(c);
                    limits64(c);
                    break;
                case EXT_MEMORY:
                    wide |= limits64(c);
                    break;
                case EXT_GLOBAL:
                    valtype(c);
                    c.byte();
                    break;
                case EXT_TAG:
                    c.byte();
                    c.uleb();
                    break;
                default:
                    return wide;
                }
            }
        }
    }
    return wide;
}

bool oom(Out &err, Str what)
{
    err.put(what).put(": out of memory");
    return false;
}

// In a linked module llvm gives every function a symbol with no name, beside
// those its name or export section gives it, so each starts a chunk.
bool unnamed(Module &m, Str what, Out &err)
{
    Cursor c(m.code);
    u32 n = c.count();
    for (u32 i = 0; i < n && c.ok(); i++) {
        NmSymbol s{};
        s.kind = SYM_FUNCTION;
        s.addr = m.code_addr + c.at();
        c.take(c.uleb());
        if (c.ok() && !m.syms.push(s))
            return oom(err, what);
    }
    if (!c.ok()) {
        err.put(what).put(": CODE +0x").hex(u32(c.where())).put(": ").put(c.why());
        return false;
    }
    return true;
}

// The code's chunks: from each function's symbol to the next, the section
// itself heading the count before the first. Of the names at one address,
// the last in order is printed.
bool code_chunks(Module &m, Str what, Out &err)
{
    Vec<const NmSymbol *> fs;
    for (const NmSymbol &s : m.syms)
        if (s.kind == SYM_FUNCTION && !s.undefined && !fs.push(&s))
            return oom(err, what);
    if (!sort(fs, before))
        return oom(err, what);
    u64 lo = m.code_addr, hi = m.code_addr + m.code.size();
    if (fs.empty() || fs[0]->addr != lo) {
        Chunk c{ lo, hi, "CODE", true, 1, 0 };
        if (!m.code_chunks.push(c))
            return oom(err, what);
    }
    for (usize i = 0; i < fs.size();) {
        usize j = i;
        while (j < fs.size() && fs[j]->addr == fs[i]->addr)
            j++;
        Chunk c{ fs[i]->addr, hi, fs[j - 1]->name, false, u32(j - i), 0 };
        if (!m.code_chunks.push(c))
            return oom(err, what);
        i = j;
    }
    // Each ends where the next begins; one with nothing in it is not printed.
    Vec<Chunk> kept;
    for (usize i = 0; i < m.code_chunks.size(); i++) {
        Chunk c = m.code_chunks[i];
        if (i + 1 < m.code_chunks.size() && m.code_chunks[i + 1].start < c.end)
            c.end = m.code_chunks[i + 1].start;
        if (c.start < lo || c.start >= c.end)
            continue;
        if (!kept.push(c))
            return oom(err, what);
    }
    m.code_chunks = move(kept);
    return true;
}

// The data's chunks: from each segment's start, and each data symbol in it.
// A symbol is in the segment it names; one from an export, in the segment
// its address falls in, since passive segments all start at 0.
bool data_chunks(Module &m, Str what, Out &err)
{
    Vec<Vec<const NmSymbol *>> in;
    if (!in.resize(m.segs.size()))
        return oom(err, what);
    for (const NmSymbol &s : m.syms) {
        if (s.kind != SYM_DATA || s.undefined)
            continue;
        u32 k = s.segment;
        for (u32 j = 0; k == ~u32(0) && j < m.segs.size(); j++)
            if (s.addr >= m.segs[j].base && s.addr < m.segs[j].base + m.segs[j].content.size())
                k = j;
        if (k < in.size() && !in[k].push(&s))
            return oom(err, what);
    }
    for (u32 k = 0; k < m.segs.size(); k++) {
        const DataSeg &g = m.segs[k];
        u64 lo = g.base, hi = g.base + g.content.size();
        Vec<const NmSymbol *> &ds = in[k];
        if (lo == hi)
            continue;
        if (!sort(ds, before))
            return oom(err, what);
        usize first = m.data_chunks.size();
        usize i     = 0;
        while (i < ds.size() && ds[i]->addr < lo)
            i++;
        if (i == ds.size() || ds[i]->addr != lo) {
            Chunk c{ lo, hi, g.name.empty() ? "DATA"_s : g.name, false, 0, k };
            if (!m.data_chunks.push(c))
                return oom(err, what);
        }
        while (i < ds.size() && ds[i]->addr < hi) {
            usize j = i;
            while (j < ds.size() && ds[j]->addr == ds[i]->addr)
                j++;
            Chunk c{ ds[i]->addr, hi, ds[j - 1]->name, false, 0, k };
            if (!m.data_chunks.push(c))
                return oom(err, what);
            i = j;
        }
        for (usize c = first; c + 1 < m.data_chunks.size(); c++)
            m.data_chunks[c].end = m.data_chunks[c + 1].start;
    }
    return true;
}

// An object's relocations of one section, by offset.
bool relocs_of(const Object &o, u32 section, Vec<RelocLine> &out, Str what, Out &err)
{
    for (const RelocSection &rs : o.relocs) {
        if (rs.target != section)
            continue;
        for (const Reloc &r : rs.relocs) {
            RelocLine l{};
            l.at     = r.offset;
            l.type   = r.type;
            l.index  = r.index;
            l.addend = r.addend;
            l.named  = r.type != R_TYPE_INDEX_LEB && r.index < o.symbols.size();
            if (l.named)
                l.symbol = o.symbols[r.index].name;
            if (!out.push(l))
                return oom(err, what);
        }
    }
    return sort(out, [](const RelocLine &a, const RelocLine &b) { return a.at < b.at; }) ||
           oom(err, what);
}

Module *open_module(const Disasm &d, Str what, Bytes file, Out &err)
{
    if (is_bitcode(file)) {
        err.put(what).put(": LLVM bitcode (from -flto), not a wasm module");
        return nullptr;
    }
    Vec<Section> sections;
    if (!read_module(what, file, sections, err))
        return nullptr;
    Module *m = heap_new<Module>();
    if (!m) {
        oom(err, what);
        return nullptr;
    }
    m->wide  = memory64(sections);
    bool ok  = read_symbols(what, file, m->syms, err);
    bool obj = is_object(sections), placed = !obj;
    for (const Section &s : sections)
        if (s.id == SEC_CUSTOM && (s.name == "dylink" || s.name == "dylink.0"))
            placed = false;
    Object o;
    if (ok && obj) {
        ok     = o.name.assign(what) || oom(err, what);
        o.file = file;
        o.link = false;
        ok     = ok && read_object(o, err);
    }
    for (u32 i = 0; ok && i < sections.size(); i++) {
        const Section &s = sections[i];
        if (s.id == SEC_CODE) {
            m->code      = s.body;
            m->code_addr = placed ? s.start : 0;
            if (!obj)
                ok = unnamed(*m, what, err);
            ok = ok && code_chunks(*m, what, err);
            if (ok && obj && d.c.relocs)
                ok = relocs_of(o, i, m->code_relocs, what, err);
        } else if (s.id == SEC_DATA && d.c.data) {
            ok = read_data(s, m->segs, err, what);
            for (u32 k = 0; ok && obj && k < m->segs.size() && k < o.segments.size(); k++)
                m->segs[k].name = o.segments[k].name;
            ok = ok && data_chunks(*m, what, err);
            if (ok && obj && d.c.relocs) {
                ok = relocs_of(o, i, m->data_relocs, what, err);
                // To addresses, through the segment each lands in.
                for (RelocLine &l : m->data_relocs)
                    for (const DataSeg &g : m->segs)
                        if (l.at >= g.content_off && l.at < g.content_off + g.content.size()) {
                            l.at = g.base + (l.at - g.content_off);
                            break;
                        }
            }
        }
    }
    if (!ok) {
        heap_delete(m);
        return nullptr;
    }
    return m;
}

void put_name(Disasm &d, Str name)
{
    if (d.c.demangle) {
        demangle(name, d.scratch);
        name = d.scratch.str();
    }
    d.out.put(name);
}

void put_label(Disasm &d, const Chunk &c)
{
    d.out.put('\n');
    put_hex(d.out, c.start, d.m->wide ? 16 : 8);
    d.out.put(" <");
    put_name(d, c.name);
    d.out.put(">:\n");
}

void put_reloc(Disasm &d, const RelocLine &l, u64 addr)
{
    d.out.put(d.m->wide ? "\t\t"_s : "\t\t\t"_s);
    put_hex(d.out, addr, d.m->wide ? 16 : 8);
    d.out.put(":  ").put(reloc_name(l.type)).put('\t');
    if (l.named)
        d.out.put(l.symbol);
    else
        put_u64(d.out, l.index);
    if (l.addend >= 0)
        d.out.put('+');
    put_i64(d.out, l.addend);
    d.out.put('\n');
}

// WebAssemblyDisassembler::onSymbolStart: the section's count of functions,
// or a function's size and locals. False where they cannot be read, and what
// was printed of them stays, without its newline.
bool header(Disasm &d, const Chunk &c, Bytes b, usize &size)
{
    Out &o = d.out;
    Cursor r(b);
    if (c.section) {
        u64 n = r.uleb();
        if (!r.ok())
            return false;
        o.put("        # ");
        put_i64(o, i64(n));
        o.put(" functions in section.\n");
        size = r.at();
        return true;
    }
    r.uleb();
    u32 entries = r.uleb();
    if (!r.ok())
        return false;
    if (entries)
        o.put("        .local ");
    bool first = true;
    for (u32 i = 0; i < entries; i++) {
        u32 n    = r.uleb();
        u32 type = r.uleb();
        if (!r.ok())
            return false;
        for (u32 j = 0; j < n; j++) {
            if (!first)
                o.put(", ");
            first = false;
            static const struct {
                u32 type;
                Str name;
            } TYPES[] = { { I32, "i32" },
                          { I64, "i64" },
                          { F32, "f32" },
                          { F64, "f64" },
                          { V128, "v128" },
                          { FUNCREF, "funcref" },
                          { EXTERNREF, "externref" },
                          { 0x69, "exnref" },
                          { 0x60, "func" },
                          { 0x40, "void" } };
            Str t     = "invalid_type";
            for (const auto &k : TYPES)
                if (k.type == type)
                    t = k.name;
            o.put(t);
        }
    }
    o.put('\n');
    size = r.at();
    return true;
}

// The zero bytes at the start of `b`, as llvm-objdump skips them: eight or
// more, in fours.
usize zeros(Bytes b, usize least, usize step)
{
    usize n = 0;
    while (n < b.size() && !b[n])
        n++;
    return n < least ? 0 : n / step * step;
}

void code_chunk(Disasm &d, Module &m, const Chunk &c)
{
    Out &o = d.out;
    if (m.next == 0)
        o.put("\nDisassembly of section CODE:\n");
    put_label(d, c);
    usize start = c.start - m.code_addr, end = c.end - m.code_addr;
    usize size = 0;
    for (u32 i = 0; i < c.symbols; i++)
        if (header(d, c, m.code.subspan(start, end - start), size)) {
            start += size;
            break;
        }
    Vec<RelocLine> &rel = m.code_relocs;
    while (m.reloc < rel.size() && rel[m.reloc].at < start)
        m.reloc++;
    usize tab = d.c.raw ? 24 : 16;
    Out text, notes, line;
    for (usize at = start; at < end;) {
        u64 most = end - at;
        if (m.reloc < rel.size() && rel[m.reloc].at - at < most)
            most = rel[m.reloc].at - at;
        if (usize n = zeros(m.code.subspan(at, most), 8, 4)) {
            o.put("\t\t...\n");
            at += n;
            continue;
        }
        text.clear();
        notes.clear();
        bool ok;
        usize n = decode(m.code.subspan(at), m.flow, text, notes, ok);
        if (n == 0)
            n = 1;
        // The address, the bytes, and the instruction a tab stop on.
        line.clear();
        put_rhex(line, m.code_addr + at, 8);
        line.put(':');
        if (d.c.raw)
            for (usize i = 0; i < n; i++)
                line.put(' ').hex(m.code[at + i], 2);
        usize col = line.str().size();
        for (usize i = 0, k = col < tab - 1 ? tab - 1 - col : 7 - col % 8; i < k; i++)
            line.put(' ');
        line.put(text.str());
        // Each comment line at the comment column.
        Str rest   = notes.str();
        usize ccol = 40 - 8 + tab;
        do {
            if (!rest.empty()) {
                Str note = rest.split('\n', rest);
                pad_to(line, column(line.str()), ccol);
                line.put("# ").put(note);
            }
            o.put(line.str()).put('\n');
            line.clear();
        } while (!rest.empty());
        while (m.reloc < rel.size() && rel[m.reloc].at < at + n) {
            put_reloc(d, rel[m.reloc], m.code_addr + rel[m.reloc].at);
            m.reloc++;
        }
        at += n;
    }
}

// A row: the address, sixteen bytes, and those that are printable.
void data_chunk(Disasm &d, Module &m, const Chunk &c)
{
    Out &o = d.out;
    if (!m.data_head)
        o.put("\nDisassembly of section DATA:\n");
    m.data_head = true;
    put_label(d, c);
    const DataSeg &g    = m.segs[c.seg];
    Vec<RelocLine> &rel = m.data_relocs;
    while (m.reloc < rel.size() && rel[m.reloc].at < c.start)
        m.reloc++;
    for (u64 at = c.start; at < c.end;) {
        Bytes b  = g.content.subspan(at - g.base, c.end - at);
        u64 most = b.size();
        if (m.reloc < rel.size() && rel[m.reloc].at - at < most)
            most = rel[m.reloc].at - at;
        if (usize z = zeros(b.subspan(0, most), 32, 16)) {
            o.put("\t\t...\n");
            at += z;
            continue;
        }
        usize n = b.size() < 16 ? b.size() : 16;
        put_rhex(o, at, 8);
        o.put(':');
        for (usize i = 0; i < 16; i++)
            if (i < n)
                o.put(' ').hex(b[i], 2);
            else
                o.put("   ");
        o.put("  ");
        for (usize i = 0; i < n; i++)
            o.put(b[i] >= 0x20 && b[i] < 0x7f ? char(b[i]) : '.');
        o.put('\n');
        while (m.reloc < rel.size() && rel[m.reloc].at < at + n) {
            put_reloc(d, rel[m.reloc], rel[m.reloc].at);
            m.reloc++;
        }
        at += n;
    }
}

bool add_job(Disasm &d, Str what, Bytes bytes)
{
    Disasm::Job j;
    j.bytes = bytes;
    return j.what.assign(what) && d.jobs.push(move(j));
}

} // namespace

void disasm_file(Disasm &d, Str name, Bytes file, Diag &diag)
{
    disasm_end(d);
    if (!is_archive(file)) {
        if (!add_job(d, name, file))
            diag.error("out of memory");
        return;
    }
    Vec<Member> members;
    Vec<ArchiveSymbol> index;
    Out err;
    if (!read_archive(name, file, members, index, err)) {
        diag.error(err.str());
        return;
    }
    for (const Member &m : members) {
        if (!is_wasm(m.data) && !is_bitcode(m.data))
            continue;
        String what;
        if (!what.append(name) || !what.push('(') || !what.append(m.name) || !what.push(')') ||
            !add_job(d, what.str(), m.data)) {
            diag.error("out of memory");
            return;
        }
    }
}

bool disasm_more(Disasm &d, Diag &diag)
{
    if (!d.m) {
        if (d.job == d.jobs.size())
            return false;
        const Disasm::Job &j = d.jobs[d.job++];
        Out err;
        d.m = open_module(d, j.what.str(), j.bytes, err);
        if (!d.m)
            diag.error(err.str());
        else
            d.out.put('\n').put(j.what.str()).put(":\tfile format wasm\n");
        return true;
    }
    Module &m = *d.m;
    if (!m.in_data && m.next < m.code_chunks.size()) {
        code_chunk(d, m, m.code_chunks[m.next]);
        m.next++;
        return true;
    }
    if (!m.in_data) {
        m.in_data = true;
        m.next    = 0;
        m.reloc   = 0;
    }
    if (m.next < m.data_chunks.size()) {
        data_chunk(d, m, m.data_chunks[m.next]);
        m.next++;
        return true;
    }
    heap_delete(d.m);
    d.m = nullptr;
    return true;
}

void disasm_end(Disasm &d)
{
    if (d.m)
        heap_delete(d.m);
    d.m = nullptr;
    d.jobs.clear();
    d.job = 0;
}
