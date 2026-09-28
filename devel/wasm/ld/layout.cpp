#include "layout.h"

#include "emit.h"
#include "symtab.h"

using namespace wasm;

namespace {

constexpr u64 PAGE = 65536;

u64 align_to(u64 v, u32 log2)
{
    u64 a = u64(1) << log2;
    return (v + a - 1) & ~(a - 1);
}

bool fill(Vec<u32> &v, usize n, u32 x)
{
    if (!v.resize(n))
        return false;
    for (u32 &e : v)
        e = x;
    return true;
}

// wasm-ld's order: by the bytes read from the end, larger first, and a
// string before any suffix of it.
bool before(const PoolString &a, const PoolString &b)
{
    for (u32 i = 0;; i++) {
        int x = i < a.n ? a.p[a.n - 1 - i] : -1;
        int y = i < b.n ? b.p[b.n - 1 - i] : -1;
        if (x != y)
            return x > y;
        if (x < 0)
            return false;
    }
}

// The output segment an input segment goes to.
Str out_name(Str n)
{
    if (n.starts_with(".text."))
        return ".text"_s;
    if (n.starts_with(".data."))
        return ".data"_s;
    if (n.starts_with(".bss."))
        return ".bss"_s;
    if (n.starts_with(".rodata."))
        return ".rodata"_s;
    return n;
}

// Output segments are ordered by this, stably: .bss last.
u32 rank(Str n)
{
    if (n.starts_with(".tdata"))
        return 0;
    if (n.starts_with(".rodata"))
        return 1;
    if (n.starts_with(".data"))
        return 2;
    if (n.starts_with(".bss"))
        return 4;
    return 3;
}

struct InitEntry {
    u32 priority;
    u32 function;
    u32 results;
};

struct Layouter {
    Linker &l;
    Layout &lay;
    bool oom = false;

    InputFile &file(u32 f) { return *l.files[f]; }

    Sym *find(Str name)
    {
        u32 *id = l.names.find(name);
        return id ? &l.syms[*id] : nullptr;
    }

    void error(Str what, Str name)
    {
        Out m;
        m.put(what).put(name);
        l.diag.error(m.str());
    }

    Target target(u32 f, u32 i) { return target_of(l, f, i); }

    u32 function_index(u32 f, u32 i) { return function_index_of(l, f, i); }

    // ------------------------------------------------------------ indices

    u32 shared_import(const Sym &g)
    {
        for (u32 id : lay.imports) {
            const Sym &h = l.syms[id];
            if (h.kind == SYM_FUNCTION && import_module(h) == import_module(g) &&
                import_field(h) == import_field(g) && same_sig(h.sig, g.sig))
                return h.out_index;
        }
        return NONE;
    }

    void imports()
    {
        for (u32 id = 0; id < l.syms.size(); id++) {
            Sym &g = l.syms[id];
            if (g.state != State::Undefined || !g.live || !imported(l, g))
                continue;
            if (g.kind == SYM_FUNCTION) {
                // One import serves every symbol of the same name and type.
                u32 same = shared_import(g);
                if (same != NONE) {
                    g.out_index = same;
                    continue;
                }
                g.out_index = lay.imported_functions++;
            } else if (g.kind == SYM_GLOBAL) {
                const Object &o = file(g.file).obj;
                if (o.imports[o.symbols[g.index].import].mut) {
                    error("mutable global imports are not linked here: ", g.name);
                    continue;
                }
                g.out_index = lay.imported_globals++;
            } else {
                error("table imports are not linked here: ", g.name);
                continue;
            }
            if (!lay.imports.push(id))
                oom = true;
        }
    }

    void add_function(Ref r)
    {
        if (!lay.functions.push(r))
            oom = true;
    }

    // Imports, the linker's own, then the inputs' in load order.
    void functions()
    {
        u32 next   = lay.imported_functions;
        Sym *ctors = find("__wasm_call_ctors");
        if (ctors && ctors->live) {
            ctors->out_index = next++;
            add_function(Ref{ NONE, *l.names.find("__wasm_call_ctors") });
        }
        for (u32 id : l.stubs)
            if (l.syms[id].live) {
                l.syms[id].out_index = next++;
                add_function(Ref{ NONE, id });
            }
        for (u32 f : l.objects) {
            InputFile &in = file(f);
            u32 n         = in.obj.functions.size();
            if (!fill(in.function_index, n, NONE) || !fill(in.slot, n, 0)) {
                oom = true;
                return;
            }
            for (u32 k = 0; k < n; k++)
                if (in.live_functions[k]) {
                    in.function_index[k] = next++;
                    add_function(Ref{ f, k });
                }
        }
    }

    void globals()
    {
        u32 next = lay.imported_globals;
        Sym *sp  = find("__stack_pointer");
        if (sp && sp->live) {
            sp->out_index = next++;
            if (!lay.globals.push(Ref{ NONE, *l.names.find("__stack_pointer") }))
                oom = true;
        }
        for (u32 f : l.objects) {
            InputFile &in = file(f);
            if (!fill(in.global_index, in.obj.globals.size(), NONE)) {
                oom = true;
                return;
            }
            for (u32 k = 0; k < in.obj.globals.size(); k++)
                if (in.live_globals[k]) {
                    in.global_index[k] = next++;
                    if (!lay.globals.push(Ref{ f, k }))
                        oom = true;
                }
            for (u32 k = 0; k < in.obj.tables.size(); k++)
                if (in.live_tables[k])
                    error("tables other than __indirect_function_table are not linked here: ",
                          in.obj.name.str());
        }
    }

    // ------------------------------------------------------------ relocations

    // A table slot for the function symbol i of file f names, unless it has
    // one. A stub's address is null and takes none.
    void elem(u32 f, u32 i)
    {
        u32 id = file(f).symbols[i];
        if (id != NONE && l.syms[id].stub)
            return;
        Target t = target(f, i);
        u32 *slot;
        if (t.file == NONE) {
            Sym &g = l.syms[t.index];
            if (g.out_index == NONE)
                return;
            slot = &g.slot;
        } else {
            const Object &o = file(t.file).obj;
            slot            = &file(t.file).slot[o.symbols[t.index].index - o.imported_functions];
        }
        if (*slot)
            return;
        if (!lay.elems.push(function_index(f, i)))
            oom = true;
        *slot = lay.elems.size();
    }

    // Types and table slots, as a live chunk's relocations name them, in
    // wasm-ld's scan order: each file's functions, then its segments.
    void scan(u32 f)
    {
        InputFile &in        = file(f);
        const Object &o      = in.obj;
        const u32 sections[] = { o.code_section, o.data_section };
        for (u32 section : sections)
            for (const RelocSection &rs : o.relocs) {
                if (section == NONE || rs.target != section)
                    continue;
                const Vec<u8> &live =
                    section == o.code_section ? in.live_functions : in.live_segments;
                for (const Reloc &r : rs.relocs) {
                    if (!live[r.chunk])
                        continue;
                    if (r.type == R_TYPE_INDEX_LEB && in.type_map[r.index] == NONE)
                        in.type_map[r.index] = add_type(&o.types[r.index]);
                    else if (r.type == R_TABLE_INDEX_SLEB || r.type == R_TABLE_INDEX_I32)
                        elem(f, r.index);
                }
            }
    }

    u32 add_type(const FuncType *t)
    {
        for (u32 i = 0; i < lay.types.size(); i++)
            if (same_sig(lay.types[i], t))
                return i;
        if (!lay.types.push(t))
            oom = true;
        return lay.types.size() - 1;
    }

    // Types named by relocations, then those of imports, then of functions.
    void types()
    {
        for (u32 f : l.objects) {
            InputFile &in = file(f);
            if (!fill(in.type_map, in.obj.types.size(), NONE)) {
                oom = true;
                return;
            }
            scan(f);
        }
        for (u32 id : lay.imports)
            if (l.syms[id].kind == SYM_FUNCTION)
                add_type(l.syms[id].sig);
        for (const Ref &r : lay.functions) {
            if (r.file == NONE) {
                add_type(l.syms[r.index].sig);
                continue;
            }
            InputFile &in  = file(r.file);
            u32 t          = in.obj.functions[r.index].type;
            in.type_map[t] = add_type(&in.obj.types[t]);
        }
        Sym *tab  = find("__indirect_function_table");
        lay.table = !lay.elems.empty() || (tab && tab->live);
    }

    // ------------------------------------------------------------ ctors

    // Calls each live init function, by priority, then in load order.
    void ctors()
    {
        Sym *g = find("__wasm_call_ctors");
        if (!g || !g->live)
            return;
        Vec<InitEntry> init;
        for (u32 f : l.objects)
            for (const InitFunc &fn : file(f).obj.init_funcs) {
                u32 index = function_index(f, fn.symbol);
                Target t  = target(f, fn.symbol);
                if (index == NONE || (t.file == NONE && !l.syms[t.index].live))
                    continue;
                const FuncType *sig;
                if (t.file == NONE) {
                    sig = l.syms[t.index].sig;
                } else {
                    const Object &o = file(t.file).obj;
                    u32 k           = o.symbols[t.index].index - o.imported_functions;
                    sig             = &o.types[o.functions[k].type];
                }
                if (!sig->params.empty()) {
                    error("constructor functions cannot take arguments: ",
                          file(f).obj.symbols[fn.symbol].name);
                    continue;
                }
                InitEntry e{ fn.priority, index, u32(sig->results.size()) };
                usize at = init.size();
                while (at > 0 && init[at - 1].priority > e.priority)
                    at--;
                if (!init.insert(at, e))
                    oom = true;
            }
        Vec<u8> body;
        Emit b{ body };
        b.uleb(0); // no locals
        for (const InitEntry &e : init) {
            b.byte(OP_CALL);
            b.uleb(e.function);
            for (u32 k = 0; k < e.results; k++)
                b.byte(OP_DROP);
        }
        b.byte(OP_END);
        Emit c{ lay.ctors };
        c.uleb(body.size());
        c.bytes(Bytes(body.data(), body.size()));
        if (b.oom || c.oom)
            oom = true;
    }

    // ------------------------------------------------------------ memory

    OutSegment *segment(Str name, u32 *index = nullptr)
    {
        for (u32 i = 0; i < lay.segments.size(); i++)
            if (lay.segments[i].name == name) {
                if (index)
                    *index = i;
                return &lay.segments[i];
            }
        return nullptr;
    }

    // Live input segments merged by name, the output ones ordered by kind.
    void segments()
    {
        for (u32 f : l.objects) {
            const InputFile &in = file(f);
            for (u32 k = 0; k < in.obj.segments.size(); k++) {
                Str name = out_name(in.obj.segments[k].name);
                if (!in.live_segments[k] || segment(name))
                    continue;
                OutSegment s;
                s.name = name;
                s.bss  = name.starts_with(".bss");
                if (!lay.segments.push(move(s)))
                    oom = true;
            }
        }
        for (usize i = 1; i < lay.segments.size(); i++)
            for (usize j = i; j > 0 && rank(lay.segments[j - 1].name) > rank(lay.segments[j].name);
                 j--) {
                OutSegment t        = move(lay.segments[j]);
                lay.segments[j]     = move(lay.segments[j - 1]);
                lay.segments[j - 1] = move(t);
            }
        // Each pool goes where its first segment would have.
        for (u32 f : l.objects) {
            InputFile &in = file(f);
            u32 n         = in.obj.segments.size();
            if (!fill(in.segment_out, n, NONE) || !fill(in.segment_off, n, 0) ||
                !fill(in.piece_start, n + 1, 0)) {
                oom = true;
                return;
            }
            for (u32 k = 0; k < n; k++) {
                in.piece_start[k] = in.pieces.size();
                if (!in.live_segments[k])
                    continue;
                const Segment &seg = in.obj.segments[k];
                OutSegment *s      = segment(out_name(seg.name), &in.segment_out[k]);
                if (!merges(seg)) {
                    if (!s->inputs.push(Ref{ f, k }))
                        oom = true;
                    continue;
                }
                in.segment_off[k] = NONE;
                split(f, k);
                u32 m = 0;
                while (m < s->merged.size() && s->merged[m].flags != seg.seg_flags)
                    m++;
                if (m == s->merged.size()) {
                    Merged pool;
                    pool.flags = seg.seg_flags;
                    if (!s->merged.push(move(pool)) || !s->inputs.push(Ref{ NONE, m }))
                        oom = true;
                }
                if (oom || !s->merged[m].members.push(Ref{ f, k }))
                    oom = true;
            }
            in.piece_start[n] = in.pieces.size();
        }
        if (oom)
            return;
        for (OutSegment &s : lay.segments) {
            for (Merged &m : s.merged)
                pool(m);
            for (const Ref &r : s.inputs) {
                if (r.file == NONE) {
                    s.merged[r.index].off = s.size;
                    s.size += s.merged[r.index].bytes.size();
                    continue;
                }
                InputFile &in           = file(r.file);
                const Segment &seg      = in.obj.segments[r.index];
                s.align                 = s.align > seg.align ? s.align : seg.align;
                s.size                  = u32(align_to(s.size, seg.align));
                in.segment_off[r.index] = s.size;
                s.size += seg.content.size();
            }
            for (const Merged &m : s.merged)
                for (const Ref &r : m.members) {
                    InputFile &in = file(r.file);
                    for (u32 p = in.piece_start[r.index]; p < in.piece_start[r.index + 1]; p++)
                        in.pieces[p].out += m.off;
                }
        }
    }

    // wasm-ld merges strings at -O1 and up, from byte-aligned STRINGS
    // segments that are not empty.
    bool merges(const Segment &seg) const
    {
        return l.cfg.optimize > 0 && (seg.seg_flags & SEG_STRINGS) && seg.align == 0 &&
               seg.content.size();
    }

    // Segment k of file f into its NUL-terminated strings.
    void split(u32 f, u32 k)
    {
        InputFile &in = file(f);
        Bytes c       = in.obj.segments[k].content;
        if (c[c.size() - 1] != 0) {
            Out m;
            m.put(l.file_name(f)).put(":(").put(in.obj.segments[k].name);
            m.put("): string is not null terminated");
            l.diag.error(m.str());
            return;
        }
        if (!split_strings(c, in.pieces))
            oom = true;
    }

    void pool(Merged &m)
    {
        Vec<PoolString> all;
        for (const Ref &r : m.members) {
            InputFile &in = file(r.file);
            Bytes c       = in.obj.segments[r.index].content;
            if (!gather(c, in.pieces, in.piece_start[r.index], in.piece_start[r.index + 1], all)) {
                oom = true;
                return;
            }
        }
        if (!pool_strings(all, m.bytes))
            oom = true;
    }

    void log(Str what, u64 v)
    {
        if (!l.cfg.verbose)
            return;
        Out m;
        m.put("mem: ").put(what).put(" = ").num(u32(v));
        l.diag.log(m.str());
    }

    // One of the linker's data symbols, if something refers to it.
    Sym *linker_data(Str name)
    {
        Sym *g = find(name);
        return g && g->synthetic() && g->state == State::Defined && g->kind == SYM_DATA ? g
                                                                                        : nullptr;
    }

    void set(Str name, u64 va)
    {
        if (Sym *g = linker_data(name))
            g->va = u32(va);
    }

    void too_small(Str what, u64 need)
    {
        Out m;
        m.put(what).put(" too small, ").num(u32(need)).put(" bytes needed");
        l.diag.error(m.str());
    }

    void memory()
    {
        segments();
        const Config &c = l.cfg;
        u64 ptr         = 0;
        auto stack      = [&] {
            ptr = align_to(ptr, 4);
            if (c.stack_size % 16)
                l.diag.error("stack size must be 16-byte aligned");
            log("stack size ", c.stack_size);
            log("stack base ", ptr);
            set("__stack_low", ptr);
            ptr += c.stack_size;
            lay.stack_pointer = u32(ptr);
            log("stack top  ", ptr);
            set("__stack_high", ptr);
        };
        if (c.stack_first) {
            stack();
            if (c.global_base_set) {
                if (c.global_base < ptr)
                    l.diag.error(
                        "--global-base cannot be less than stack size when --stack-first is used");
                else
                    ptr = c.global_base;
            }
        } else {
            ptr = c.global_base;
        }
        log("global base", ptr);
        set("__global_base", ptr);
        set("__dso_handle", ptr);
        u64 start = ptr;
        for (OutSegment &s : lay.segments) {
            ptr    = align_to(ptr, s.align);
            s.addr = u32(ptr);
            if (c.verbose) {
                Out m;
                m.put("mem: ").left(s.name, 15).put(" offset=").lnum(s.addr, 8);
                m.put(" size=").lnum(s.size, 8).put(" align=").num(s.align);
                l.diag.log(m.str());
            }
            ptr += s.size;
        }
        set("__data_end", ptr);
        log("static data", ptr - start);
        if (!c.stack_first)
            stack();
        if (linker_data("__heap_base")) {
            ptr = align_to(ptr, 4);
            log("heap base  ", ptr);
            set("__heap_base", ptr);
        }
        if (c.initial_memory) {
            if (c.initial_memory % PAGE)
                l.diag.error("initial memory must be aligned to the page size (65536 bytes)");
            if (ptr > c.initial_memory)
                too_small("initial memory", ptr);
            ptr = c.initial_memory;
        }
        ptr       = align_to(ptr, 16);
        lay.pages = u32(ptr / PAGE);
        log("total pages", lay.pages);
        if (linker_data("__heap_end")) {
            log("heap end   ", ptr);
            set("__heap_end", ptr);
        }
        if (c.max_memory) {
            if (c.max_memory % PAGE)
                l.diag.error("maximum memory must be aligned to the page size (65536 bytes)");
            if (ptr > c.max_memory)
                too_small("maximum memory", ptr);
            lay.max_pages = c.max_memory / PAGE;
            log("max pages  ", lay.max_pages);
        }
        if (ptr > (u64(1) << 32))
            l.diag.error("memory is larger than 4 GiB");
    }

    bool run()
    {
        imports();
        functions();
        globals();
        types();
        ctors();
        memory();
        if (oom)
            l.diag.error("out of memory");
        return !l.diag.failed();
    }
};

} // namespace

Target target_of(const Linker &l, u32 f, u32 i)
{
    u32 id = l.files[f]->symbols[i];
    if (id == NONE)
        return Target{ f, i };
    const Sym &g = l.syms[id];
    if (g.state == State::Defined && !g.synthetic())
        return Target{ g.file, g.index };
    return Target{ NONE, id };
}

u32 function_index_of(const Linker &l, u32 f, u32 i)
{
    Target t = target_of(l, f, i);
    if (t.file == NONE)
        return l.syms[t.index].out_index;
    const InputFile &in = *l.files[t.file];
    return in.function_index[in.obj.symbols[t.index].index - in.obj.imported_functions];
}

u32 type_index_of(const Layout &lay, const FuncType *t)
{
    for (u32 i = 0; i < lay.types.size(); i++)
        if (same_sig(lay.types[i], t))
            return i;
    return NONE;
}

u32 segment_offset(const InputFile &in, u32 seg, u32 off)
{
    if (in.segment_off[seg] != NONE)
        return in.segment_off[seg] + off;
    // The last piece at or before off.
    u32 lo = in.piece_start[seg], hi = in.piece_start[seg + 1];
    while (hi - lo > 1) {
        u32 mid = lo + (hi - lo) / 2;
        if (in.pieces[mid].in <= off)
            lo = mid;
        else
            hi = mid;
    }
    return in.pieces[lo].out + (off - in.pieces[lo].in);
}

Str import_module(const Sym &g)
{
    return g.import_module.empty() ? "env"_s : g.import_module;
}

Str import_field(const Sym &g)
{
    return g.import_name.empty() ? g.name : g.import_name;
}

bool layout(Linker &l)
{
    Layouter *p = heap_new<Layouter>(l, l.layout);
    if (!p) {
        l.diag.error("out of memory");
        return false;
    }
    bool ok = p->run();
    heap_delete(p);
    return ok;
}

// ---------------------------------------------------------------- the dump

namespace {

// A file's symbols that its live chunks define, grouped by chunk in symbol
// order: those of chunk k are list[start[k]] to list[start[k + 1]].
struct Buckets {
    Vec<u32> start;
    Vec<u32> list;
};

// Whether symbol i of file f is a live definition of this file's, and in
// which chunk.
bool defines(const Linker &l, u32 f, u32 i, u8 kind, u32 &chunk)
{
    const InputFile &in = *l.files[f];
    const Symbol &s     = in.obj.symbols[i];
    if (s.undefined() || s.kind != kind)
        return false;
    u32 id = in.symbols[i];
    if (id != NONE) {
        const Sym &g = l.syms[id];
        if (g.state != State::Defined || g.file != f || g.index != i)
            return false;
    }
    if (kind == SYM_FUNCTION) {
        chunk = s.index - in.obj.imported_functions;
        return in.live_functions[chunk];
    }
    if (s.flags & SYM_ABSOLUTE)
        return false;
    chunk = s.segment;
    return in.live_segments[chunk];
}

bool bucket(const Linker &l, u32 f, u8 kind, u32 chunks, Buckets &b)
{
    const Object &o = l.files[f]->obj;
    if (!b.start.resize(chunks + 2))
        return false;
    for (u32 &s : b.start)
        s = 0;
    u32 chunk, n = 0;
    for (u32 i = 0; i < o.symbols.size(); i++)
        if (defines(l, f, i, kind, chunk)) {
            b.start[chunk + 2]++;
            n++;
        }
    for (u32 k = 2; k < b.start.size(); k++)
        b.start[k] += b.start[k - 1];
    if (!b.list.resize(n))
        return false;
    for (u32 i = 0; i < o.symbols.size(); i++)
        if (defines(l, f, i, kind, chunk))
            b.list[b.start[chunk + 1]++] = i;
    return true;
}

Str function_name(const Linker &l, u32 index, Out &tmp)
{
    const Layout &lay = l.layout;
    if (index < lay.imported_functions) {
        for (u32 id : lay.imports)
            if (l.syms[id].kind == SYM_FUNCTION && l.syms[id].out_index == index)
                return l.syms[id].name;
        return "?"_s;
    }
    const Ref &r = lay.functions[index - lay.imported_functions];
    String dm;
    tmp.clear();
    if (r.file != NONE)
        return tmp.put(shown(l.cfg, l.files[r.file]->obj.functions[r.index].name, dm)).str();
    const Sym &g = l.syms[r.index];
    if (g.stub)
        tmp.put(g.mismatch ? "signature_mismatch:" : "undefined_weak:");
    return tmp.put(shown(l.cfg, g.name, dm)).str();
}

// A line's first columns: the address, or "-" for none; with -Map's file
// offset when `off` is given; the size; then the indent.
Out &row(Out &out, u32 addr, bool has_addr, u32 size, u32 indent, const u32 *off = nullptr)
{
    if (has_addr)
        out.rhex(addr, 8);
    else
        out.put("       -");
    if (off)
        out.put(' ').rhex(*off, 8);
    out.put(' ').rhex(size, 8).put(' ');
    for (u32 i = 0; i < indent; i++)
        out.put(' ');
    return out;
}

void global_rows(const Linker &l, Out &out, bool map)
{
    const Layout &lay = l.layout;
    u32 zero          = 0;
    for (u32 i = 0; i < lay.globals.size(); i++) {
        const Ref &r = lay.globals[i];
        row(out, lay.imported_globals + i, true, 0, 8, map ? &zero : nullptr);
        out.put(r.file == NONE ? l.syms[r.index].name : l.files[r.file]->obj.globals[r.index].name);
        out.put('\n');
    }
}

// Functions and their symbols. -Map counts a function's offset from the
// CODE section's start plus its place in the body, the header not counted.
void code_rows(const Linker &l, Out &out, bool map)
{
    String dm;
    const Layout &lay = l.layout;
    const Map &m      = lay.map;
    Buckets b;
    u32 last = NONE;
    for (u32 i = 0; i < lay.functions.size(); i++) {
        const Ref &r = lay.functions[i];
        u32 off      = map ? m.code + m.code_off[i] : 0;
        const u32 *o = map ? &off : nullptr;
        u32 size;
        if (map)
            size = m.code_off[i + 1] - m.code_off[i];
        else if (r.file == NONE)
            size = l.syms[r.index].stub ? 4 : lay.ctors.size();
        else {
            const Function &f = l.files[r.file]->obj.functions[r.index];
            size              = f.body_off - f.code_off + f.body_size;
        }
        if (r.file == NONE) {
            row(out, 0, false, size, 8, o).put("<internal>:(").put(l.syms[r.index].name).put(")\n");
            continue;
        }
        const InputFile &in = *l.files[r.file];
        const Object &obj   = in.obj;
        if (r.file != last && !bucket(l, r.file, SYM_FUNCTION, obj.functions.size(), b))
            out.oom = true;
        last = r.file;
        row(out, 0, false, size, 8, o).put(obj.name.str()).put(":(");
        out.put(obj.functions[r.index].name).put(")\n");
        for (u32 k = b.start[r.index]; k < b.start[r.index + 1]; k++)
            row(out, 0, false, size, 16, o)
                .put(shown(l.cfg, obj.symbols[b.list[k]].name, dm))
                .put('\n');
    }
}

// Segments, their inputs and their symbols. An unwritten segment's inputs
// have no file offset, and their symbols count from 0.
void data_rows(const Linker &l, Out &out, bool map)
{
    String dm;
    const Layout &lay = l.layout;
    const Map &m      = lay.map;
    Buckets b;
    u32 last = NONE;
    for (u32 i = 0; i < lay.segments.size(); i++) {
        const OutSegment &s = lay.segments[i];
        bool written        = map && m.seg_data[i];
        u32 off             = map ? m.data + m.seg_off[i] : 0;
        row(out, s.addr, true, s.size, 0, map ? &off : nullptr).put(s.name).put('\n');
        for (const Ref &r : s.inputs) {
            if (r.file == NONE) {
                const Merged &mg = s.merged[r.index];
                u32 at           = written ? m.data + m.seg_data[i] + mg.off : 0;
                row(out, s.addr + mg.off, true, mg.bytes.size(), 8, map ? &at : nullptr);
                out.put("<internal>:(").put(s.name).put(")\n");
                continue;
            }
            const InputFile &in = *l.files[r.file];
            const Object &o     = in.obj;
            if (r.file != last && !bucket(l, r.file, SYM_DATA, o.segments.size(), b))
                out.oom = true;
            last               = r.file;
            const Segment &seg = o.segments[r.index];
            u32 addr           = s.addr + in.segment_off[r.index];
            u32 at             = written ? m.data + m.seg_data[i] + in.segment_off[r.index] : 0;
            row(out, addr, true, seg.content.size(), 8, map ? &at : nullptr);
            out.put(o.name.str()).put(":(").put(seg.name).put(")\n");
            for (u32 k = b.start[r.index]; k < b.start[r.index + 1]; k++) {
                const Symbol &sym = o.symbols[b.list[k]];
                u32 sat           = at + sym.offset;
                row(out, addr + sym.offset, true, sym.size, 16, map ? &sat : nullptr);
                out.put(shown(l.cfg, sym.name, dm)).put('\n');
            }
        }
    }
}

} // namespace

void dump_layout(const Linker &l, Out &out)
{
    const Layout &lay = l.layout;
    for (const FuncType *t : lay.types) {
        out.put("type ");
        put_sig(out, t);
        out.put('\n');
    }
    if (l.cfg.import_memory)
        out.put("import env.memory memory\n");
    for (u32 id : lay.imports) {
        const Sym &g = l.syms[id];
        out.put("import ").put(import_module(g)).put('.').put(import_field(g));
        out.put(g.kind == SYM_FUNCTION ? " function\n" : " global\n");
    }
    for (u32 i = 0; i < lay.globals.size(); i++) {
        const Ref &r = lay.globals[i];
        out.put("global ").num(lay.imported_globals + i).put(' ');
        if (r.file == NONE) {
            out.put("i32 mut ").num(lay.stack_pointer).put(' ').put(l.syms[r.index].name);
        } else {
            const Global &g = l.files[r.file]->obj.globals[r.index];
            out.put(valtype_name(g.valtype)).put(g.mut ? " mut " : " const ");
            if (g.init.is_i32)
                out.snum(g.init.value);
            else
                out.put('-');
            out.put(' ').put(g.name);
        }
        out.put('\n');
    }
    if (lay.table)
        out.put("table ").num(lay.elems.size() + 1).put(' ').num(lay.elems.size() + 1).put('\n');
    Out tmp;
    for (u32 i = 0; i < lay.elems.size(); i++)
        out.put("elem ").num(i + 1).put(' ').put(function_name(l, lay.elems[i], tmp)).put('\n');

    // __wasm_call_ctors: its size, no locals, then a call and drops each.
    usize at = 0;
    while (at < lay.ctors.size() && (lay.ctors[at] & 0x80))
        at++;
    for (at += 2; at < lay.ctors.size() && lay.ctors[at] == OP_CALL;) {
        u32 index = 0, shift = 0;
        u8 b;
        do {
            b = lay.ctors[++at];
            index |= u32(b & 0x7f) << shift;
            shift += 7;
        } while (b & 0x80);
        out.put("ctor ").put(function_name(l, index, tmp)).put('\n');
        for (at++; at < lay.ctors.size() && lay.ctors[at] == OP_DROP; at++)
            ;
    }

    if (!lay.globals.empty()) {
        out.put("GLOBAL\n");
        global_rows(l, out, false);
    }
    out.put("CODE\n");
    code_rows(l, out, false);
    if (lay.segments.empty())
        return;
    out.put("DATA\n");
    data_rows(l, out, false);
}

void write_map(const Linker &l, Out &out)
{
    static const Str NAMES[] = { "CUSTOM", "TYPE",   "IMPORT",   "FUNCTION", "TABLE",
                                 "MEMORY", "GLOBAL", "EXPORT",   "START",    "ELEM",
                                 "CODE",   "DATA",   "DATACOUNT" };
    out.put("    Addr      Off     Size Out     In      Symbol\n");
    for (const MapSection &s : l.layout.map.sections) {
        u32 off = s.off;
        row(out, 0, false, s.size, 0, &off).put(NAMES[s.id]);
        if (s.id == SEC_CUSTOM)
            out.put('(').put(s.name).put(')');
        out.put('\n');
        if (s.id == SEC_GLOBAL)
            global_rows(l, out, true);
        else if (s.id == SEC_CODE)
            code_rows(l, out, true);
        else if (s.id == SEC_DATA)
            data_rows(l, out, true);
    }
}

bool split_strings(Bytes c, Vec<Piece> &out)
{
    for (u32 at = 0; at < c.size();) {
        if (!out.push(Piece{ at, 0 }))
            return false;
        while (at < c.size() && c[at])
            at++;
        at++;
    }
    return true;
}

bool gather(Bytes c, Vec<Piece> &pieces, u32 from, u32 to, Vec<PoolString> &all)
{
    for (u32 p = from; p < to; p++) {
        u32 end = p + 1 < to ? pieces[p + 1].in : c.size();
        if (!all.push(PoolString{ &pieces[p], c.data() + pieces[p].in, end - pieces[p].in }))
            return false;
    }
    return true;
}

bool pool_strings(const Vec<PoolString> &all, Vec<u8> &bytes)
{
    // Bottom-up merge sort: no recursion, and the sort is total but for
    // equal strings, which land alike.
    Vec<u32> a, b;
    if (!fill(a, all.size(), 0) || !fill(b, all.size(), 0))
        return false;
    for (u32 i = 0; i < all.size(); i++)
        a[i] = i;
    for (u32 w = 1; w < all.size(); w *= 2) {
        for (u32 lo = 0; lo < all.size(); lo += 2 * w) {
            u32 mid = lo + w < all.size() ? lo + w : all.size();
            u32 hi  = lo + 2 * w < all.size() ? lo + 2 * w : all.size();
            u32 i = lo, j = mid, o = lo;
            while (i < mid && j < hi)
                b[o++] = before(all[a[j]], all[a[i]]) ? a[j++] : a[i++];
            while (i < mid)
                b[o++] = a[i++];
            while (j < hi)
                b[o++] = a[j++];
        }
        Vec<u32> t = move(a);
        a          = move(b);
        b          = move(t);
    }
    const PoolString *prev = nullptr;
    for (u32 i : a) {
        const PoolString &s = all[i];
        bool tail           = prev && prev->n >= s.n;
        for (u32 k = 0; tail && k < s.n; k++)
            tail = prev->p[prev->n - s.n + k] == s.p[k];
        if (tail) {
            s.piece->out = bytes.size() - s.n;
            continue;
        }
        s.piece->out = bytes.size();
        for (u32 k = 0; k < s.n; k++)
            if (!bytes.push(s.p[k]))
                return false;
        prev = &s;
    }
    return true;
}
