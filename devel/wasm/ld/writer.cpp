#include "writer.h"

#include "emit.h"
#include "kernel/alloc.h"
#include "stamp.h"
#include "symtab.h"

using namespace wasm;

namespace {

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

struct Writer {
    Linker &l;
    const Layout &lay;
    Vec<u8> &out;
    Vec<u8> sec; // the section being built
    Emit eo{ out };
    Emit es{ sec };
    bool oom = false;

    // A local encoder's failure, kept as the writer's.
    void fold(const Emit &e) { oom = oom || e.oom; }

    bool out_of_memory() const { return oom || eo.oom || es.oom; }

    const InputFile &file(u32 f) { return *l.files[f]; }

    void error(Str what, Str name)
    {
        Out m;
        m.put(what).put(name);
        l.diag.error(m.str());
    }

    Map &map() { return l.layout.map; }

    // Notes for -Map the section written since `at`, if one was.
    void note(usize at, Str name = Str())
    {
        if (out.size() == at)
            return;
        if (!map().sections.push(MapSection{ out[at], name, u32(at), u32(out.size() - at) }))
            oom = true;
    }

    // The section built in `sec`, behind its id and size.
    void section(u8 id)
    {
        emit_section(eo, id, Bytes(sec.data(), sec.size()));
        sec.clear();
    }

    void custom(Str title)
    {
        emit_custom(eo, title, Bytes(sec.data(), sec.size()));
        sec.clear();
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
        v                     = seg.addr + segment_offset(in, s.segment, s.offset) + u32(addend);
        return true;
    }

    enum class Form { ULEB, SLEB, I32 };

    // Relocation r of file f: its value, and how it is encoded.
    bool value(u32 f, const Reloc &r, u32 &v, Form &form)
    {
        v = 0;
        switch (r.type) {
        case R_FUNCTION_INDEX_LEB:
        case R_FUNCTION_INDEX_I32:
            v    = function_index_of(l, f, r.index);
            form = r.type == R_FUNCTION_INDEX_LEB ? Form::ULEB : Form::I32;
            if (v == NONE) {
                error("no function for ", file(f).obj.symbols[r.index].name);
                return false;
            }
            break;
        case R_TABLE_INDEX_SLEB:
        case R_TABLE_INDEX_I32:
            v    = slot_of(f, r.index);
            form = r.type == R_TABLE_INDEX_SLEB ? Form::SLEB : Form::I32;
            break;
        case R_MEMORY_ADDR_LEB:
        case R_MEMORY_ADDR_SLEB:
        case R_MEMORY_ADDR_I32:
            if (!address_of(f, r.index, r.addend, v)) {
                error("not a data symbol: ", file(f).obj.symbols[r.index].name);
                return false;
            }
            form = r.type == R_MEMORY_ADDR_LEB    ? Form::ULEB
                   : r.type == R_MEMORY_ADDR_SLEB ? Form::SLEB
                                                  : Form::I32;
            break;
        case R_TYPE_INDEX_LEB:
            v    = file(f).type_map[r.index];
            form = Form::ULEB;
            break;
        case R_GLOBAL_INDEX_LEB:
        case R_GLOBAL_INDEX_I32:
            v    = global_of(f, r.index);
            form = r.type == R_GLOBAL_INDEX_LEB ? Form::ULEB : Form::I32;
            break;
        case R_TABLE_NUMBER_LEB:
            v    = 0;
            form = Form::ULEB;
            break;
        default:
            error("relocation outside debug info: ", reloc_name(r.type));
            return false;
        }
        return true;
    }

    // Writes relocation r of file f at `at`: padded LEBs and little-endian
    // words, so nothing moves.
    void apply(u32 f, const Reloc &r, u8 *at)
    {
        u32 v;
        Form form;
        if (value(f, r, v, form))
            put(form, v, at);
    }

    // Padded LEBs and little-endian words, so nothing moves.
    void put(Form form, u32 v, u8 *at)
    {
        if (form == Form::I32) {
            for (u32 k = 0; k < 4; k++)
                at[k] = u8(v >> (8 * k));
            return;
        }
        // Five bytes, whatever the value: the field was padded to five.
        i32 sv = i32(v);
        for (u32 k = 0; k < 5; k++) {
            u8 b  = form == Form::ULEB ? u8((v >> (7 * k)) & 0x7f) : u8((sv >> (7 * k)) & 0x7f);
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
        es.uleb(lay.types.size());
        for (const FuncType *t : lay.types) {
            es.byte(FUNC_TYPE);
            es.uleb(t->params.size());
            es.bytes(t->params);
            es.uleb(t->results.size());
            es.bytes(t->results);
        }
        section(SEC_TYPE);
    }

    void imports()
    {
        u32 n = lay.imports.size() + (l.cfg.import_memory ? 1 : 0);
        if (!n)
            return;
        es.uleb(n);
        if (l.cfg.import_memory) {
            es.name("env");
            es.name("memory");
            es.byte(EXT_MEMORY);
            limits(lay.pages, lay.max_pages);
        }
        for (u32 id : lay.imports) {
            const Sym &g = l.syms[id];
            es.name(import_module(g));
            es.name(import_field(g));
            if (g.kind == SYM_FUNCTION) {
                es.byte(EXT_FUNCTION);
                es.uleb(type_of(g.sig));
            } else {
                const Object &o  = file(g.file).obj;
                const Import &im = o.imports[o.symbols[g.index].import];
                es.byte(EXT_GLOBAL);
                es.byte(im.valtype);
                es.byte(im.mut ? 1 : 0);
            }
        }
        section(SEC_IMPORT);
    }

    void limits(u32 min, u32 max)
    {
        es.byte(max ? 1 : 0);
        es.uleb(min);
        if (max)
            es.uleb(max);
    }

    void functions()
    {
        es.uleb(lay.functions.size());
        for (const Ref &r : lay.functions)
            es.uleb(type_of(sig_of(r)));
        section(SEC_FUNCTION);
    }

    void table()
    {
        if (!lay.table)
            return;
        u32 n = lay.elems.size() + 1;
        es.uleb(1);
        es.byte(FUNCREF);
        es.byte(1);
        es.uleb(n);
        es.uleb(n);
        section(SEC_TABLE);
    }

    void memory()
    {
        if (l.cfg.import_memory)
            return;
        es.uleb(1);
        limits(lay.pages, lay.max_pages);
        section(SEC_MEMORY);
    }

    void globals()
    {
        if (lay.globals.empty())
            return;
        es.uleb(lay.globals.size());
        for (const Ref &r : lay.globals) {
            if (r.file == NONE) {
                es.byte(I32);
                es.byte(1);
                es.byte(OP_I32_CONST);
                es.sleb(i32(lay.stack_pointer));
                es.byte(OP_END);
                continue;
            }
            const Global &g = file(r.file).obj.globals[r.index];
            es.byte(g.valtype);
            es.byte(g.mut ? 1 : 0);
            es.bytes(g.init.code);
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
        Emit el{ list };
        u32 n = 0;
        if (!l.cfg.import_memory) {
            el.name("memory");
            el.byte(EXT_MEMORY);
            el.uleb(0);
            n++;
        }
        for (const Sym &g : l.syms) {
            if (g.state != State::Defined || !((g.flags & SYM_EXPORTED) || forced(g.name)))
                continue;
            if (g.kind == SYM_FUNCTION) {
                el.name(export_name(g));
                el.byte(EXT_FUNCTION);
                if (g.synthetic()) {
                    el.uleb(g.out_index);
                } else {
                    const InputFile &in = file(g.file);
                    u32 k               = in.obj.symbols[g.index].index - in.obj.imported_functions;
                    el.uleb(in.function_index[k]);
                }
            } else if (g.kind == SYM_GLOBAL) {
                el.name(g.name);
                el.byte(EXT_GLOBAL);
                if (g.synthetic()) {
                    el.uleb(g.out_index);
                } else {
                    const InputFile &in = file(g.file);
                    el.uleb(
                        in.global_index[in.obj.symbols[g.index].index - in.obj.imported_globals]);
                }
            } else {
                error("only functions and globals are exported here: ", g.name);
                continue;
            }
            n++;
        }
        fold(el);
        if (!n)
            return;
        es.uleb(n);
        es.bytes(Bytes(list.data(), list.size()));
        section(SEC_EXPORT);
    }

    void elems()
    {
        if (lay.elems.empty())
            return;
        es.uleb(1);
        es.uleb(0); // active, table 0
        es.byte(OP_I32_CONST);
        es.sleb(1);
        es.byte(OP_END);
        es.uleb(lay.elems.size());
        for (u32 f : lay.elems)
            es.uleb(f);
        section(SEC_ELEM);
    }

    // --compress-relocations: function r's body with every relocated LEB
    // at its shortest, behind its new size, appended to `to`.
    void pack(const Ref &r, Vec<u8> &to)
    {
        const Object &o        = file(r.file).obj;
        const Function &f      = o.functions[r.index];
        Bytes body             = o.sections[o.code_section].body.subspan(f.body_off, f.body_size);
        const RelocSection *rs = relocs_for(o, o.code_section);
        Vec<u8> packed;
        Emit ep{ packed };
        Emit et{ to };
        u32 last = 0;
        usize k  = 0;
        if (rs) {
            usize hi = rs->relocs.size();
            while (k < hi) {
                usize mid = (k + hi) / 2;
                if (rs->relocs[mid].chunk < r.index)
                    k = mid + 1;
                else
                    hi = mid;
            }
        }
        for (; rs && k < rs->relocs.size() && rs->relocs[k].chunk == r.index; k++) {
            const Reloc &x = rs->relocs[k];
            ep.bytes(body.subspan(last, x.at - last));
            u32 v;
            Form form;
            if (!value(r.file, x, v, form))
                return;
            if (form == Form::I32) {
                error("relocation compression not supported for ", reloc_name(x.type));
                return;
            }
            if (form == Form::ULEB)
                ep.uleb(v);
            else
                ep.sleb64(i64(v));
            last = x.at + 5;
        }
        ep.bytes(body.subspan(last, body.size() - last));
        et.uleb(packed.size());
        et.bytes(Bytes(packed.data(), packed.size()));
        fold(ep);
        fold(et);
    }

    u32 chunk_size(const Ref &r)
    {
        if (r.file == NONE)
            return l.syms[r.index].stub ? 4 : lay.ctors.size();
        const Function &f = file(r.file).obj.functions[r.index];
        return f.body_off - f.code_off + f.body_size;
    }

    // Written in place: its size is known, so the bodies are not copied twice.
    // Packed bodies are built first, since only packing says how long they are.
    void code()
    {
        Vec<u8> packed;
        Vec<u32> packed_at;
        if (l.cfg.compress_relocations)
            for (const Ref &r : lay.functions)
                if (r.file != NONE) {
                    if (!packed_at.push(packed.size()))
                        oom = true;
                    pack(r, packed);
                }
        if (!packed_at.push(packed.size()))
            oom = true;
        if (out_of_memory() || l.diag.failed())
            return;
        u32 size = uleb_size(lay.functions.size());
        for (const Ref &r : lay.functions)
            size += l.cfg.compress_relocations && r.file != NONE ? 0 : chunk_size(r);
        size += packed.size();
        map().code = out.size();
        eo.byte(SEC_CODE);
        eo.uleb(size);
        if (!out.reserve(out.size() + size)) {
            oom = true;
            return;
        }
        usize start = out.size();
        eo.uleb(lay.functions.size());
        static const u8 STUB[] = { 3, 0, OP_UNREACHABLE, OP_END };
        u32 n                  = 0;
        for (const Ref &r : lay.functions) {
            if (!map().code_off.push(out.size() - start))
                oom = true;
            if (l.cfg.compress_relocations && r.file != NONE) {
                eo.bytes(Bytes(packed.data() + packed_at[n], packed_at[n + 1] - packed_at[n]));
                n++;
                continue;
            }
            if (r.file == NONE) {
                if (l.syms[r.index].stub)
                    eo.bytes(Bytes(STUB, sizeof STUB));
                else
                    eo.bytes(Bytes(lay.ctors.data(), lay.ctors.size()));
                continue;
            }
            const Object &o   = file(r.file).obj;
            const Function &f = o.functions[r.index];
            Bytes body        = o.sections[o.code_section].body;
            usize at          = out.size();
            eo.bytes(body.subspan(f.code_off, chunk_size(r)));
            if (out_of_memory())
                return;
            relocate(r.file, relocs_for(o, o.code_section), r.index,
                     out.data() + at + (f.body_off - f.code_off));
        }
        if (!map().code_off.push(out.size() - start))
            oom = true;
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
                Emit eh{ head };
                eh.sleb(i32(s.addr));
                fold(eh);
                size += 1 + 1 + head.size() + 1 + uleb_size(s.size) + s.size;
            }
        if (!count)
            return;
        size += uleb_size(count);
        map().data = out.size();
        eo.byte(SEC_DATA);
        eo.uleb(size);
        if (!out.reserve(out.size() + size)) {
            oom = true;
            return;
        }
        usize start = out.size();
        eo.uleb(count);
        for (const OutSegment &s : lay.segments) {
            if (!map().seg_off.push(written(s) ? out.size() - start : 0))
                oom = true;
            if (!written(s)) {
                if (!map().seg_data.push(0))
                    oom = true;
                continue;
            }
            eo.uleb(0);
            eo.byte(OP_I32_CONST);
            eo.sleb(i32(s.addr));
            eo.byte(OP_END);
            eo.uleb(s.size);
            usize base = out.size();
            if (!map().seg_data.push(base - start))
                oom = true;
            for (u32 k = 0; k < s.size; k++)
                eo.byte(0);
            if (out_of_memory())
                return;
            for (const Ref &r : s.inputs) {
                if (r.file == NONE) {
                    const Merged &m = s.merged[r.index];
                    for (usize k = 0; k < m.bytes.size(); k++)
                        out[base + m.off + k] = m.bytes[k];
                    continue;
                }
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

    // What the linker writes itself, or never copies.
    static bool own_section(Str name)
    {
        return name == "linking" || name == "name" || name == "producers" ||
               name == "target_features" || name.starts_with("reloc.") || name == ".llvmbc" ||
               name == ".llvmcmd";
    }

    // Merged as data strings are, at -O1 and up.
    static bool string_section(Str name)
    {
        return name == ".debug_str" || name == ".debug_str.dwo" || name == ".debug_line_str";
    }

    // An output custom section: its inputs in order, and the one pool of
    // their merged strings, which goes where the first merged input would.
    struct Custom {
        Str name;
        Vec<Ref> chunks;    // file, section
        Vec<Ref> merged;    // file, section
        u32 pool_at = NONE; // the pool comes before chunks[pool_at]
        Vec<u8> pool;
    };
    Vec<Custom> customs;

    // wasm-ld's calculateCustomSections: every input's custom sections, by
    // name in the order first met, but its own, the empty, those of a comdat
    // not kept, and debug info when stripped.
    void plan_customs()
    {
        bool strip = l.cfg.strip_debug || l.cfg.strip_all;
        for (u32 f : l.objects) {
            InputFile &in   = *l.files[f];
            const Object &o = in.obj;
            u32 n           = o.sections.size();
            if (!in.custom_off.resize(n) || !in.custom_piece_start.resize(n + 1)) {
                oom = true;
                return;
            }
            for (u32 k = 0; k < n; k++) {
                in.custom_off[k]         = NONE;
                in.custom_piece_start[k] = in.custom_pieces.size();
                const Section &s         = o.sections[k];
                if (s.id != SEC_CUSTOM || s.body.empty() || own_section(s.name) ||
                    (s.comdat != NONE && !in.kept[s.comdat]) ||
                    (strip && s.name.starts_with(".debug_")))
                    continue;
                u32 c = 0;
                while (c < customs.size() && customs[c].name != s.name)
                    c++;
                if (c == customs.size()) {
                    Custom x;
                    x.name = s.name;
                    if (!customs.push(move(x))) {
                        oom = true;
                        return;
                    }
                }
                Custom &x = customs[c];
                if (l.cfg.optimize == 0 || !string_section(s.name)) {
                    if (!x.chunks.push(Ref{ f, k }))
                        oom = true;
                    continue;
                }
                if (s.body[s.body.size() - 1] != 0) {
                    Out m;
                    m.put(l.file_name(f)).put(":(").put(s.name);
                    m.put("): string is not null terminated");
                    l.diag.error(m.str());
                    continue;
                }
                if (x.pool_at == NONE)
                    x.pool_at = x.chunks.size();
                if (!x.merged.push(Ref{ f, k }) || !split_strings(s.body, in.custom_pieces))
                    oom = true;
            }
            in.custom_piece_start[n] = in.custom_pieces.size();
        }
        for (Custom &x : customs) {
            Vec<PoolString> all;
            for (const Ref &r : x.merged) {
                InputFile &in = *l.files[r.file];
                if (!gather(in.obj.sections[r.index].body, in.custom_pieces,
                            in.custom_piece_start[r.index], in.custom_piece_start[r.index + 1],
                            all))
                    oom = true;
            }
            if (!pool_strings(all, x.pool))
                oom = true;
            u32 size = 0;
            for (u32 i = 0; i <= x.chunks.size(); i++) {
                if (i == x.pool_at)
                    size += x.pool.size();
                if (i == x.chunks.size())
                    break;
                InputFile &in                    = *l.files[x.chunks[i].file];
                in.custom_off[x.chunks[i].index] = size;
                size += in.obj.sections[x.chunks[i].index].body.size();
            }
        }
    }

    // What a relocation into section k of file f at `off` resolves to.
    // A merged string counts from its pool, wherever the pool is; a section
    // that is not written, from 0.
    u32 section_offset(u32 f, u32 k, u32 off)
    {
        const InputFile &in = file(f);
        if (in.custom_off.size() <= k)
            return off;
        if (in.custom_off[k] != NONE)
            return in.custom_off[k] + off;
        u32 lo = in.custom_piece_start[k], hi = in.custom_piece_start[k + 1];
        if (lo == hi)
            return off;
        while (hi - lo > 1) {
            u32 mid = lo + (hi - lo) / 2;
            if (in.custom_pieces[mid].in <= off)
                lo = mid;
            else
                hi = mid;
        }
        return in.custom_pieces[lo].out + (off - in.custom_pieces[lo].in);
    }

    // Whether what symbol i of file f names was kept: lld's isLive().
    bool is_live(u32 f, u32 i)
    {
        Target t = target_of(l, f, i);
        if (t.file == NONE)
            return l.syms[t.index].live;
        const InputFile &in = file(t.file);
        const Object &o     = in.obj;
        const Symbol &s     = o.symbols[t.index];
        switch (s.kind) {
        case SYM_FUNCTION:
            return in.live_functions[s.index - o.imported_functions];
        case SYM_DATA:
            return (s.flags & SYM_ABSOLUTE) || in.live_segments[s.segment];
        case SYM_GLOBAL:
            return in.live_globals[s.index - o.imported_globals];
        case SYM_TABLE:
            return in.live_tables[s.index - o.imported_tables];
        default:
            return true;
        }
    }

    // A relocation in a custom section: lld's calcNewValue, where what is
    // dead reads as the section's tombstone, or else as the addend.
    bool custom_value(u32 f, const Reloc &r, u32 tombstone, u32 &v, Form &form)
    {
        const Object &o = file(f).obj;
        form = reloc_is_i32(r.type) ? Form::I32 : reloc_is_sleb(r.type) ? Form::SLEB : Form::ULEB;
        if (r.type != R_TYPE_INDEX_LEB && o.symbols[r.index].kind != SYM_SECTION &&
            !is_live(f, r.index)) {
            v = tombstone ? tombstone : u32(r.addend);
            return true;
        }
        if (r.type == R_SECTION_OFFSET_I32) {
            v = section_offset(f, o.symbols[r.index].index, u32(r.addend));
            return true;
        }
        if (r.type != R_FUNCTION_OFFSET_I32)
            return value(f, r, v, form);
        Target t = target_of(l, f, r.index);
        if (t.file == NONE && l.syms[t.index].state != State::Defined && !l.syms[t.index].stub) {
            v = tombstone ? tombstone : u32(r.addend);
            return true;
        }
        u32 k    = function_index_of(l, f, r.index) - lay.imported_functions;
        u32 lebn = 1;
        if (t.file != NONE) {
            const Object &to  = file(t.file).obj;
            const Function &x = to.functions[to.symbols[t.index].index - to.imported_functions];
            lebn              = x.body_off - x.code_off;
        }
        v = map().code_off[k] + lebn + u32(r.addend);
        return true;
    }

    static u32 tombstone_of(Str name)
    {
        if (name == ".debug_ranges" || name == ".debug_loc")
            return 0xfffffffe;
        if (name.starts_with(".debug_") || name.starts_with("llvm.func_attr."))
            return 0xffffffff;
        return 0;
    }

    void copy_customs()
    {
        plan_customs();
        if (out_of_memory() || l.diag.failed())
            return;
        for (Custom &x : customs) {
            u32 tomb = tombstone_of(x.name);
            for (u32 i = 0; i <= x.chunks.size(); i++) {
                if (i == x.pool_at)
                    es.bytes(Bytes(x.pool.data(), x.pool.size()));
                if (i == x.chunks.size())
                    break;
                const Ref &r    = x.chunks[i];
                const Object &o = file(r.file).obj;
                usize base      = sec.size();
                es.bytes(o.sections[r.index].body);
                const RelocSection *rs = relocs_for(o, r.index);
                for (usize k = 0; rs && !out_of_memory() && k < rs->relocs.size(); k++) {
                    const Reloc &rel = rs->relocs[k];
                    u32 v;
                    Form form;
                    if (custom_value(r.file, rel, tomb, v, form))
                        put(form, v, sec.data() + base + rel.at);
                }
            }
            usize at = out.size();
            custom(x.name);
            note(at, x.name);
        }
    }

    String shown_; // what the last shown name was demangled into

    Str shown_name(Str name) { return shown(l.cfg, name, shown_); }

    Str function_name(const Ref &r, Out &tmp)
    {
        if (r.file != NONE)
            return shown_name(file(r.file).obj.functions[r.index].name);
        const Sym &g = l.syms[r.index];
        if (!g.stub)
            return shown_name(g.name);
        tmp.clear();
        tmp.put(g.mismatch ? "signature_mismatch:" : "undefined_weak:").put(shown_name(g.name));
        return tmp.str();
    }

    void subsection(u8 id, Vec<u8> &part)
    {
        emit_section(es, id, Bytes(part.data(), part.size()));
        part.clear();
    }

    void names()
    {
        Vec<u8> part;
        Emit ep{ part };
        Out tmp;
        // The module is named for the file it is written to, as wasm-ld names it.
        Str module  = l.cfg.output.empty() ? "a.out"_s : l.cfg.output;
        usize slash = module.size();
        while (slash > 0 && module[slash - 1] != '/')
            slash--;
        ep.name(module.substr(slash));
        subsection(0, part);

        ep.uleb(lay.imported_functions + lay.functions.size());
        for (u32 id : lay.imports)
            if (l.syms[id].kind == SYM_FUNCTION) {
                ep.uleb(l.syms[id].out_index);
                ep.name(shown_name(l.syms[id].name));
            }
        for (u32 i = 0; i < lay.functions.size(); i++) {
            ep.uleb(lay.imported_functions + i);
            ep.name(function_name(lay.functions[i], tmp));
        }
        subsection(1, part);

        if (lay.imported_globals + lay.globals.size()) {
            ep.uleb(lay.imported_globals + lay.globals.size());
            for (u32 id : lay.imports)
                if (l.syms[id].kind == SYM_GLOBAL) {
                    ep.uleb(l.syms[id].out_index);
                    ep.name(shown_name(l.syms[id].name));
                }
            for (u32 i = 0; i < lay.globals.size(); i++) {
                const Ref &r = lay.globals[i];
                ep.uleb(lay.imported_globals + i);
                ep.name(shown_name(r.file == NONE ? l.syms[r.index].name
                                                  : file(r.file).obj.globals[r.index].name));
            }
            subsection(7, part);
        }

        u32 n = 0;
        for (const OutSegment &s : lay.segments)
            n += written(s);
        if (n) {
            ep.uleb(n);
            u32 i = 0;
            for (const OutSegment &s : lay.segments)
                if (written(s)) {
                    ep.uleb(i++);
                    ep.name(s.name);
                }
            subsection(9, part);
        }
        fold(ep);
        custom("name");
    }

    // Each field's tools, first seen first, one per name.
    void producers()
    {
        static const Str FIELDS[] = { "language", "processed-by", "sdk" };
        u32 fields                = 0;
        Vec<u8> body;
        Emit eb{ body };
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
            eb.name(field);
            eb.uleb(seen.size());
            for (const Producer *p : seen) {
                eb.name(p->name);
                eb.name(p->version);
            }
        }
        fold(eb);
        if (!fields)
            return;
        es.uleb(fields);
        es.bytes(Bytes(body.data(), body.size()));
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
        es.uleb(used.size());
        for (Str s : used) {
            es.byte('+');
            es.name(s);
        }
        custom("target_features");
    }

    void stamp()
    {
        const Config &c = l.cfg;
        emit_stamp(eo, Stamp{ BRAAM_MAGIC, c.braam_abi, 0,
                              c.braam_initial ? c.braam_initial : lay.pages, c.braam_max });
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
        eo.bytes(Bytes(HEADER, sizeof HEADER));
        void (Writer::*const STANDARD[])() = {
            &Writer::types,   &Writer::imports, &Writer::functions, &Writer::table, &Writer::memory,
            &Writer::globals, &Writer::exports, &Writer::elems,     &Writer::code,  &Writer::data,
        };
        for (auto step : STANDARD) {
            usize at = out.size();
            (this->*step)();
            note(at);
        }
        copy_customs();
        usize at = out.size();
        // --strip-all takes the linker's own custom sections too.
        if (!l.cfg.strip_all) {
            names();
            note(at, "name");
            at = out.size();
            producers();
            note(at, "producers");
            at = out.size();
            features();
            note(at, "target_features");
            at = out.size();
        }
        stamp();
        note(at, "braam");
        if (out_of_memory())
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
