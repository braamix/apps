#include "writer.h"

#include "kernel/alloc.h"
#include "symtab.h"

using namespace wasm;

namespace {

constexpr u32 BRAAM_MAGIC = 0x6d617262;

enum ExternKind : u8 { EXT_FUNCTION = 0, EXT_TABLE = 1, EXT_MEMORY = 2, EXT_GLOBAL = 3 };

// Byte order, as std::set<std::string> sorts.
bool before(Str a, Str b)
{
    usize n = a.size() < b.size() ? a.size() : b.size();
    for (usize i = 0; i < n; i++)
        if (a[i] != b[i])
            return u8(a[i]) < u8(b[i]);
    return a.size() < b.size();
}

u32 uleb_size(u32 v)
{
    u32 n = 1;
    while (v >>= 7)
        n++;
    return n;
}

struct Writer {
    Linker &l;
    const Layout &lay;
    Vec<u8> &out;
    Vec<u8> sec; // the section being built
    bool oom = false;

    const InputFile &file(u32 f) { return *l.files[f]; }

    void error(Str what, Str name)
    {
        Out m;
        m.put(what).put(name);
        l.diag.error(m.str());
    }

    // ------------------------------------------------------------ encoding

    void byte(Vec<u8> &v, u8 b)
    {
        if (!v.push(b))
            oom = true;
    }

    void uleb(Vec<u8> &v, u32 x)
    {
        do {
            u8 b = x & 0x7f;
            x >>= 7;
            byte(v, u8(b | (x ? 0x80 : 0)));
        } while (x);
    }

    void sleb(Vec<u8> &v, i32 x)
    {
        for (;;) {
            u8 b = x & 0x7f;
            x >>= 7;
            bool done = (x == 0 && !(b & 0x40)) || (x == -1 && (b & 0x40));
            byte(v, u8(b | (done ? 0 : 0x80)));
            if (done)
                return;
        }
    }

    void u32le(Vec<u8> &v, u32 x)
    {
        for (u32 k = 0; k < 4; k++)
            byte(v, u8(x >> (8 * k)));
    }

    void bytes(Vec<u8> &v, Bytes b)
    {
        if (!v.reserve(v.size() + b.size()))
            oom = true;
        for (u8 c : b)
            byte(v, c);
    }

    void name(Vec<u8> &v, Str s)
    {
        uleb(v, s.size());
        bytes(v, Bytes(reinterpret_cast<const u8 *>(s.data()), s.size()));
    }

    // The section built in `sec`, behind its id and size.
    void section(u8 id)
    {
        byte(out, id);
        uleb(out, sec.size());
        bytes(out, Bytes(sec.data(), sec.size()));
        sec.clear();
    }

    void custom(Str title)
    {
        Vec<u8> body;
        name(body, title);
        bytes(body, Bytes(sec.data(), sec.size()));
        sec.clear();
        bytes(sec, Bytes(body.data(), body.size()));
        section(SEC_CUSTOM);
    }

    // ------------------------------------------------------------ values

    u32 type_of(const FuncType *t) { return type_index_of(lay, t); }

    const FuncType *sig_of(const Ref &r)
    {
        if (r.file == NONE)
            return l.syms[r.index].sig;
        const Object &o = file(r.file).obj;
        return &o.types[o.functions[r.index].type];
    }

    u32 slot_of(u32 f, u32 i)
    {
        u32 id = file(f).symbols[i];
        if (id != NONE && l.syms[id].stub)
            return 0;
        Target t = target_of(l, f, i);
        if (t.file == NONE)
            return l.syms[t.index].slot;
        const Object &o = file(t.file).obj;
        return file(t.file).slot[o.symbols[t.index].index - o.imported_functions];
    }

    u32 global_of(u32 f, u32 i)
    {
        Target t = target_of(l, f, i);
        if (t.file == NONE)
            return l.syms[t.index].out_index;
        const Object &o = file(t.file).obj;
        return file(t.file).global_index[o.symbols[t.index].index - o.imported_globals];
    }

    // A data symbol's address; an undefined weak one's is 0, addend and all.
    bool address_of(u32 f, u32 i, i32 addend, u32 &v)
    {
        Target t = target_of(l, f, i);
        if (t.file == NONE) {
            const Sym &g = l.syms[t.index];
            if (g.state != State::Defined) {
                v = 0;
                return true;
            }
            v = g.va + u32(addend);
            return true;
        }
        const InputFile &in = file(t.file);
        const Symbol &s     = in.obj.symbols[t.index];
        if (s.kind != SYM_DATA)
            return false;
        if (s.flags & SYM_ABSOLUTE) {
            v = s.offset + u32(addend);
            return true;
        }
        const OutSegment &seg = lay.segments[in.segment_out[s.segment]];
        v                     = seg.addr + in.segment_off[s.segment] + s.offset + u32(addend);
        return true;
    }

    // Writes relocation r of file f at `at`: padded LEBs and little-endian
    // words, so nothing moves.
    void apply(u32 f, const Reloc &r, u8 *at)
    {
        u32 v = 0;
        enum { ULEB, SLEB, I32 } form;
        switch (r.type) {
        case R_FUNCTION_INDEX_LEB:
        case R_FUNCTION_INDEX_I32:
            v    = function_index_of(l, f, r.index);
            form = r.type == R_FUNCTION_INDEX_LEB ? ULEB : I32;
            if (v == NONE)
                return error("no function for ", file(f).obj.symbols[r.index].name);
            break;
        case R_TABLE_INDEX_SLEB:
        case R_TABLE_INDEX_I32:
            v    = slot_of(f, r.index);
            form = r.type == R_TABLE_INDEX_SLEB ? SLEB : I32;
            break;
        case R_MEMORY_ADDR_LEB:
        case R_MEMORY_ADDR_SLEB:
        case R_MEMORY_ADDR_I32:
            if (!address_of(f, r.index, r.addend, v))
                return error("not a data symbol: ", file(f).obj.symbols[r.index].name);
            form = r.type == R_MEMORY_ADDR_LEB ? ULEB : r.type == R_MEMORY_ADDR_SLEB ? SLEB : I32;
            break;
        case R_TYPE_INDEX_LEB:
            v    = file(f).type_map[r.index];
            form = ULEB;
            break;
        case R_GLOBAL_INDEX_LEB:
        case R_GLOBAL_INDEX_I32:
            v    = global_of(f, r.index);
            form = r.type == R_GLOBAL_INDEX_LEB ? ULEB : I32;
            break;
        case R_TABLE_NUMBER_LEB:
            v    = 0;
            form = ULEB;
            break;
        default:
            return error("relocation outside debug info: ", reloc_name(r.type));
        }
        if (form == I32) {
            for (u32 k = 0; k < 4; k++)
                at[k] = u8(v >> (8 * k));
            return;
        }
        // Five bytes, whatever the value: the field was padded to five.
        i32 sv = i32(v);
        for (u32 k = 0; k < 5; k++) {
            u8 b  = form == ULEB ? u8((v >> (7 * k)) & 0x7f) : u8((sv >> (7 * k)) & 0x7f);
            at[k] = u8(b | (k < 4 ? 0x80 : 0));
        }
    }

    // Relocations of one chunk: those of `rs` whose chunk is `chunk`, into
    // `base`, where the chunk's relocation offsets count from.
    void relocate(u32 f, const RelocSection *rs, u32 chunk, u8 *base)
    {
        if (!rs)
            return;
        usize lo = 0, hi = rs->relocs.size();
        while (lo < hi) {
            usize mid = (lo + hi) / 2;
            if (rs->relocs[mid].chunk < chunk)
                lo = mid + 1;
            else
                hi = mid;
        }
        for (usize k = lo; k < rs->relocs.size() && rs->relocs[k].chunk == chunk; k++)
            apply(f, rs->relocs[k], base + rs->relocs[k].at);
    }

    const RelocSection *relocs_for(const Object &o, u32 section)
    {
        for (const RelocSection &rs : o.relocs)
            if (rs.target == section)
                return &rs;
        return nullptr;
    }

    // ------------------------------------------------------------ sections

    void types()
    {
        uleb(sec, lay.types.size());
        for (const FuncType *t : lay.types) {
            byte(sec, FUNC_TYPE);
            uleb(sec, t->params.size());
            bytes(sec, t->params);
            uleb(sec, t->results.size());
            bytes(sec, t->results);
        }
        section(SEC_TYPE);
    }

    void imports()
    {
        u32 n = lay.imports.size() + (l.cfg.import_memory ? 1 : 0);
        if (!n)
            return;
        uleb(sec, n);
        if (l.cfg.import_memory) {
            name(sec, "env");
            name(sec, "memory");
            byte(sec, EXT_MEMORY);
            limits(lay.pages, lay.max_pages);
        }
        for (u32 id : lay.imports) {
            const Sym &g = l.syms[id];
            name(sec, import_module(g));
            name(sec, import_field(g));
            if (g.kind == SYM_FUNCTION) {
                byte(sec, EXT_FUNCTION);
                uleb(sec, type_of(g.sig));
            } else {
                const Object &o  = file(g.file).obj;
                const Import &im = o.imports[o.symbols[g.index].import];
                byte(sec, EXT_GLOBAL);
                byte(sec, im.valtype);
                byte(sec, im.mut ? 1 : 0);
            }
        }
        section(SEC_IMPORT);
    }

    void limits(u32 min, u32 max)
    {
        byte(sec, max ? 1 : 0);
        uleb(sec, min);
        if (max)
            uleb(sec, max);
    }

    void functions()
    {
        uleb(sec, lay.functions.size());
        for (const Ref &r : lay.functions)
            uleb(sec, type_of(sig_of(r)));
        section(SEC_FUNCTION);
    }

    void table()
    {
        if (!lay.table)
            return;
        u32 n = lay.elems.size() + 1;
        uleb(sec, 1);
        byte(sec, FUNCREF);
        byte(sec, 1);
        uleb(sec, n);
        uleb(sec, n);
        section(SEC_TABLE);
    }

    void memory()
    {
        if (l.cfg.import_memory)
            return;
        uleb(sec, 1);
        limits(lay.pages, lay.max_pages);
        section(SEC_MEMORY);
    }

    void globals()
    {
        if (lay.globals.empty())
            return;
        uleb(sec, lay.globals.size());
        for (const Ref &r : lay.globals) {
            if (r.file == NONE) {
                byte(sec, I32);
                byte(sec, 1);
                byte(sec, OP_I32_CONST);
                sleb(sec, i32(lay.stack_pointer));
                byte(sec, OP_END);
                continue;
            }
            const Global &g = file(r.file).obj.globals[r.index];
            byte(sec, g.valtype);
            byte(sec, g.mut ? 1 : 0);
            bytes(sec, g.init.code);
        }
        section(SEC_GLOBAL);
    }

    bool forced(Str name)
    {
        for (Str e : l.cfg.exports)
            if (e == name)
                return true;
        return false;
    }

    // An input function's export name: its object's, else its symbol's.
    Str export_name(const Sym &g)
    {
        if (g.synthetic() || g.kind != SYM_FUNCTION)
            return g.name;
        const Object &o = file(g.file).obj;
        u32 index       = o.symbols[g.index].index;
        for (const Export &e : o.exports)
            if (e.kind == EXT_FUNCTION && e.index == index)
                return e.name;
        return g.name;
    }

    void exports()
    {
        Vec<u8> list;
        u32 n = 0;
        if (!l.cfg.import_memory) {
            name(list, "memory");
            byte(list, EXT_MEMORY);
            uleb(list, 0);
            n++;
        }
        for (const Sym &g : l.syms) {
            if (g.state != State::Defined || !((g.flags & SYM_EXPORTED) || forced(g.name)))
                continue;
            if (g.kind == SYM_FUNCTION) {
                name(list, export_name(g));
                byte(list, EXT_FUNCTION);
                if (g.synthetic()) {
                    uleb(list, g.out_index);
                } else {
                    const InputFile &in = file(g.file);
                    u32 k               = in.obj.symbols[g.index].index - in.obj.imported_functions;
                    uleb(list, in.function_index[k]);
                }
            } else if (g.kind == SYM_GLOBAL) {
                name(list, g.name);
                byte(list, EXT_GLOBAL);
                if (g.synthetic()) {
                    uleb(list, g.out_index);
                } else {
                    const InputFile &in = file(g.file);
                    uleb(list,
                         in.global_index[in.obj.symbols[g.index].index - in.obj.imported_globals]);
                }
            } else {
                error("only functions and globals are exported here: ", g.name);
                continue;
            }
            n++;
        }
        if (!n)
            return;
        uleb(sec, n);
        bytes(sec, Bytes(list.data(), list.size()));
        section(SEC_EXPORT);
    }

    void elems()
    {
        if (lay.elems.empty())
            return;
        uleb(sec, 1);
        uleb(sec, 0); // active, table 0
        byte(sec, OP_I32_CONST);
        sleb(sec, 1);
        byte(sec, OP_END);
        uleb(sec, lay.elems.size());
        for (u32 f : lay.elems)
            uleb(sec, f);
        section(SEC_ELEM);
    }

    u32 chunk_size(const Ref &r)
    {
        if (r.file == NONE)
            return l.syms[r.index].stub ? 4 : lay.ctors.size();
        const Function &f = file(r.file).obj.functions[r.index];
        return f.body_off - f.code_off + f.body_size;
    }

    // Written in place: its size is known, so the bodies are not copied twice.
    void code()
    {
        u32 size = uleb_size(lay.functions.size());
        for (const Ref &r : lay.functions)
            size += chunk_size(r);
        byte(out, SEC_CODE);
        uleb(out, size);
        if (!out.reserve(out.size() + size)) {
            oom = true;
            return;
        }
        uleb(out, lay.functions.size());
        static const u8 STUB[] = { 3, 0, OP_UNREACHABLE, OP_END };
        for (const Ref &r : lay.functions) {
            if (r.file == NONE) {
                if (l.syms[r.index].stub)
                    bytes(out, Bytes(STUB, sizeof STUB));
                else
                    bytes(out, Bytes(lay.ctors.data(), lay.ctors.size()));
                continue;
            }
            const Object &o   = file(r.file).obj;
            const Function &f = o.functions[r.index];
            Bytes body        = o.sections[o.code_section].body;
            usize at          = out.size();
            bytes(out, body.subspan(f.code_off, chunk_size(r)));
            if (oom)
                return;
            relocate(r.file, relocs_for(o, o.code_section), r.index,
                     out.data() + at + (f.body_off - f.code_off));
        }
    }

    // The output segments that carry bytes: .bss is zeros, and a Braam
    // process starts on zeroed memory.
    bool written(const OutSegment &s) { return !s.bss && s.size; }

    void data()
    {
        u32 count = 0, size = 0;
        for (const OutSegment &s : lay.segments)
            if (written(s)) {
                count++;
                // flags, i32.const, address, end, size, bytes
                Vec<u8> head;
                sleb(head, i32(s.addr));
                size += 1 + 1 + head.size() + 1 + uleb_size(s.size) + s.size;
            }
        if (!count)
            return;
        size += uleb_size(count);
        byte(out, SEC_DATA);
        uleb(out, size);
        if (!out.reserve(out.size() + size)) {
            oom = true;
            return;
        }
        uleb(out, count);
        for (const OutSegment &s : lay.segments) {
            if (!written(s))
                continue;
            uleb(out, 0);
            byte(out, OP_I32_CONST);
            sleb(out, i32(s.addr));
            byte(out, OP_END);
            uleb(out, s.size);
            usize base = out.size();
            for (u32 k = 0; k < s.size; k++)
                byte(out, 0);
            if (oom)
                return;
            for (const Ref &r : s.inputs) {
                const InputFile &in = file(r.file);
                const Segment &seg  = in.obj.segments[r.index];
                u8 *at              = out.data() + base + in.segment_off[r.index];
                for (usize k = 0; k < seg.content.size(); k++)
                    at[k] = seg.content[k];
                relocate(r.file, relocs_for(in.obj, in.obj.data_section), r.index, at);
            }
        }
    }

    // ------------------------------------------------------------ custom

    Str function_name(const Ref &r, Out &tmp)
    {
        if (r.file != NONE)
            return file(r.file).obj.functions[r.index].name;
        const Sym &g = l.syms[r.index];
        if (!g.stub)
            return g.name;
        tmp.clear();
        tmp.put("undefined_weak:").put(g.name);
        return tmp.str();
    }

    void subsection(Vec<u8> &body, u8 id, Vec<u8> &part)
    {
        byte(body, id);
        uleb(body, part.size());
        bytes(body, Bytes(part.data(), part.size()));
        part.clear();
    }

    void names()
    {
        Vec<u8> part;
        Out tmp;
        // The module is named for the file it is written to, as wasm-ld names it.
        Str module  = l.cfg.output.empty() ? "a.out"_s : l.cfg.output;
        usize slash = module.size();
        while (slash > 0 && module[slash - 1] != '/')
            slash--;
        name(part, module.substr(slash));
        subsection(sec, 0, part);

        uleb(part, lay.imported_functions + lay.functions.size());
        for (u32 id : lay.imports)
            if (l.syms[id].kind == SYM_FUNCTION) {
                uleb(part, l.syms[id].out_index);
                name(part, l.syms[id].name);
            }
        for (u32 i = 0; i < lay.functions.size(); i++) {
            uleb(part, lay.imported_functions + i);
            name(part, function_name(lay.functions[i], tmp));
        }
        subsection(sec, 1, part);

        if (lay.imported_globals + lay.globals.size()) {
            uleb(part, lay.imported_globals + lay.globals.size());
            for (u32 id : lay.imports)
                if (l.syms[id].kind == SYM_GLOBAL) {
                    uleb(part, l.syms[id].out_index);
                    name(part, l.syms[id].name);
                }
            for (u32 i = 0; i < lay.globals.size(); i++) {
                const Ref &r = lay.globals[i];
                uleb(part, lay.imported_globals + i);
                name(part, r.file == NONE ? l.syms[r.index].name
                                          : file(r.file).obj.globals[r.index].name);
            }
            subsection(sec, 7, part);
        }

        u32 n = 0;
        for (const OutSegment &s : lay.segments)
            n += written(s);
        if (n) {
            uleb(part, n);
            u32 i = 0;
            for (const OutSegment &s : lay.segments)
                if (written(s)) {
                    uleb(part, i++);
                    name(part, s.name);
                }
            subsection(sec, 9, part);
        }
        custom("name");
    }

    // Each field's tools, first seen first, one per name.
    void producers()
    {
        static const Str FIELDS[] = { "language", "processed-by", "sdk" };
        u32 fields                = 0;
        Vec<u8> body;
        for (Str field : FIELDS) {
            Vec<const Producer *> seen;
            for (u32 f : l.objects)
                for (const Producer &p : file(f).obj.producers) {
                    if (p.field != field)
                        continue;
                    bool dup = false;
                    for (const Producer *q : seen)
                        dup = dup || q->name == p.name;
                    if (!dup && !seen.push(&p))
                        oom = true;
                }
            if (seen.empty())
                continue;
            fields++;
            name(body, field);
            uleb(body, seen.size());
            for (const Producer *p : seen) {
                name(body, p->name);
                name(body, p->version);
            }
        }
        if (!fields)
            return;
        uleb(sec, fields);
        bytes(sec, Bytes(body.data(), body.size()));
        custom("producers");
    }

    // The features any input uses, sorted; none may be one another input
    // disallows.
    void features()
    {
        Vec<Str> used;
        for (u32 f : l.objects)
            for (const Feature &x : file(f).obj.features) {
                if (x.prefix == '-')
                    continue;
                usize at = 0;
                while (at < used.size() && before(used[at], x.name))
                    at++;
                if ((at == used.size() || used[at] != x.name) && !used.insert(at, x.name))
                    oom = true;
            }
        for (u32 f : l.objects)
            for (const Feature &x : file(f).obj.features) {
                if (x.prefix != '-')
                    continue;
                for (u32 g : l.objects)
                    for (const Feature &y : file(g).obj.features)
                        if (y.prefix != '-' && y.name == x.name) {
                            Out m;
                            m.put("Target feature '").put(x.name).put("' used in ");
                            m.put(file(g).obj.name.str()).put(" is disallowed by ");
                            m.put(file(f).obj.name.str());
                            m.put(". Use --no-check-features to suppress.");
                            l.diag.error(m.str());
                        }
            }
        if (used.empty())
            return;
        uleb(sec, used.size());
        for (Str s : used) {
            byte(sec, '+');
            name(sec, s);
        }
        custom("target_features");
    }

    void stamp()
    {
        const Config &c = l.cfg;
        u32le(sec, BRAAM_MAGIC);
        u32le(sec, c.braam_abi);
        u32le(sec, 0);
        u32le(sec, c.braam_initial ? c.braam_initial : lay.pages);
        u32le(sec, c.braam_max);
        custom("braam");
    }

    bool run()
    {
        static const u8 HEADER[] = { 0, 'a', 's', 'm', 1, 0, 0, 0 };
        u32 estimate             = 4096;
        for (const Ref &r : lay.functions)
            estimate += chunk_size(r);
        for (const OutSegment &s : lay.segments)
            estimate += written(s) ? s.size : 0;
        if (!out.reserve(estimate))
            oom = true;
        bytes(out, Bytes(HEADER, sizeof HEADER));
        types();
        imports();
        functions();
        table();
        memory();
        globals();
        exports();
        elems();
        code();
        data();
        names();
        producers();
        features();
        stamp();
        if (oom)
            l.diag.error("out of memory");
        return !l.diag.failed();
    }
};

} // namespace

bool write_module(Linker &l, Vec<u8> &out)
{
    Writer *w = heap_new<Writer>(l, l.layout, out);
    if (!w) {
        l.diag.error("out of memory");
        return false;
    }
    bool ok = w->run();
    heap_delete(w);
    return ok;
}
