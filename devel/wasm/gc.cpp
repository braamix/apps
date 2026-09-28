#include "gc.h"

#include "kernel/alloc.h"

using namespace wasm;

namespace {

enum Kind : u8 { FUNC, SEG, GLOB, TAB };

struct Item {
    u32 file;
    u8 kind;
    u32 index;
};

struct Marker {
    Linker &l;
    Vec<Item> queue;
    bool oom = false;

    InputFile &file(u32 f) { return *l.files[f]; }

    u8 &bit(u32 f, u8 kind, u32 i)
    {
        InputFile &in = file(f);
        switch (kind) {
        case FUNC:
            return in.live_functions[i];
        case SEG:
            return in.live_segments[i];
        case GLOB:
            return in.live_globals[i];
        default:
            return in.live_tables[i];
        }
    }

    // The chunk a defined symbol lives in; false for none, or for one its
    // comdat discarded.
    bool chunk_of(const InputFile &in, const Symbol &s, u8 &kind, u32 &i)
    {
        const Object &o = in.obj;
        u32 comdat      = NONE;
        switch (s.kind) {
        case SYM_FUNCTION:
            kind   = FUNC;
            i      = s.index - o.imported_functions;
            comdat = o.functions[i].comdat;
            break;
        case SYM_DATA:
            if (s.flags & SYM_ABSOLUTE)
                return false;
            kind   = SEG;
            i      = s.segment;
            comdat = o.segments[i].comdat;
            break;
        case SYM_GLOBAL:
            kind = GLOB;
            i    = s.index - o.imported_globals;
            break;
        case SYM_TABLE:
            kind = TAB;
            i    = s.index - o.imported_tables;
            break;
        default:
            return false;
        }
        return comdat == NONE || in.kept[comdat];
    }

    void push_chunk(u32 f, u8 kind, u32 i)
    {
        u8 &b = bit(f, kind, i);
        if (b)
            return;
        b = 1;
        if ((kind == FUNC || kind == SEG) && !queue.push(Item{ f, kind, i }))
            oom = true;
    }

    // What a file brings once something it defines is live.
    void implicit_deps(u32 f)
    {
        const Object &o = file(f).obj;
        for (const InitFunc &fn : o.init_funcs)
            enqueue_ref(f, fn.symbol);
        for (u32 k = 0; k < o.segments.size(); k++) {
            const Segment &s = o.segments[k];
            if ((s.seg_flags & SEG_RETAIN) && (s.comdat == NONE || file(f).kept[s.comdat]))
                push_chunk(f, SEG, k);
        }
    }

    // Symbol i, defined in file f.
    void enqueue_def(u32 f, u32 i)
    {
        InputFile &in   = file(f);
        const Symbol &s = in.obj.symbols[i];
        u8 kind;
        u32 k;
        bool has = chunk_of(in, s, kind, k);
        if (!has && s.kind != SYM_DATA && s.kind != SYM_SECTION)
            return;
        if (has && bit(f, kind, k))
            return;
        bool first = !in.live;
        in.live    = true;
        if (has)
            push_chunk(f, kind, k);
        if (first)
            implicit_deps(f);
    }

    void enqueue_sym(u32 id)
    {
        Sym &g = l.syms[id];
        g.live = true;
        if (g.state == State::Defined && !g.synthetic())
            enqueue_def(g.file, g.index);
    }

    // Symbol i of file f, as its relocations name it.
    void enqueue_ref(u32 f, u32 i)
    {
        u32 id = file(f).symbols[i];
        if (id == NONE)
            enqueue_def(f, i);
        else
            enqueue_sym(id);
    }

    bool sym_live(u32 f, u32 i)
    {
        u32 id = file(f).symbols[i];
        if (id != NONE) {
            const Sym &g = l.syms[id];
            if (g.state != State::Defined || g.synthetic())
                return g.live;
            f = g.file;
            i = g.index;
        }
        u8 kind;
        u32 k;
        return chunk_of(file(f), file(f).obj.symbols[i], kind, k) && bit(f, kind, k);
    }

    // A stub's address is null, so taking it does not keep the stub.
    bool stub_address(u32 f, const Reloc &r)
    {
        if (r.type != R_TABLE_INDEX_SLEB && r.type != R_TABLE_INDEX_I32)
            return false;
        u32 id = file(f).symbols[r.index];
        return id != NONE && l.syms[id].stub;
    }

    void walk(const Item &it)
    {
        const Object &o = file(it.file).obj;
        u32 section     = it.kind == FUNC ? o.code_section : o.data_section;
        for (const RelocSection &rs : o.relocs) {
            if (rs.target != section)
                continue;
            // In offset order, so in chunk order too.
            usize lo = 0, hi = rs.relocs.size();
            while (lo < hi) {
                usize mid = (lo + hi) / 2;
                if (rs.relocs[mid].chunk < it.index)
                    lo = mid + 1;
                else
                    hi = mid;
            }
            for (usize r = lo; r < rs.relocs.size() && rs.relocs[r].chunk == it.index; r++) {
                const Reloc &x = rs.relocs[r];
                if (reloc_symbol_kind(x.type) != SYM_NONE && !stub_address(it.file, x))
                    enqueue_ref(it.file, x.index);
            }
        }
    }

    bool run()
    {
        for (u32 f : l.objects)
            file(f).live = !file(f).member;

        if (!l.cfg.entry.empty())
            if (u32 *id = l.names.find(l.cfg.entry))
                enqueue_sym(*id);
        for (u32 id = 0; id < l.syms.size(); id++) {
            const Sym &g = l.syms[id];
            if ((g.flags & SYM_NO_STRIP) || (g.state == State::Defined && (g.flags & SYM_EXPORTED)))
                enqueue_sym(id);
        }
        for (Str name : l.cfg.exports)
            if (u32 *id = l.names.find(name))
                enqueue_sym(*id);
        for (u32 f : l.objects)
            if (file(f).live)
                implicit_deps(f);

        while (!queue.empty() && !oom) {
            Item it = queue.back();
            queue.pop();
            walk(it);
        }

        // __wasm_call_ctors calls whatever init function is live.
        for (u32 f : l.objects)
            for (const InitFunc &fn : file(f).obj.init_funcs)
                if (sym_live(f, fn.symbol))
                    enqueue_sym(*l.names.find("__wasm_call_ctors"));
        return !oom;
    }
};

bool fill(Vec<u8> &v, usize n, u8 x)
{
    if (!v.resize(n))
        return false;
    for (u8 &b : v)
        b = x;
    return true;
}

} // namespace

bool mark_live(Linker &l)
{
    u8 all = !l.cfg.gc_sections;
    for (u32 f : l.objects) {
        InputFile &in   = *l.files[f];
        const Object &o = in.obj;
        if (!fill(in.live_functions, o.functions.size(), all) ||
            !fill(in.live_segments, o.segments.size(), all) ||
            !fill(in.live_globals, o.globals.size(), all) ||
            !fill(in.live_tables, o.tables.size(), all)) {
            l.diag.error("out of memory");
            return false;
        }
    }
    if (all) {
        for (u32 f : l.objects)
            l.files[f]->live = true;
        for (Sym &g : l.syms)
            g.live = true;
        return true;
    }
    Marker *m = heap_new<Marker>(l);
    bool ok   = m && m->run();
    heap_delete(m);
    if (!ok)
        l.diag.error("out of memory");
    return ok;
}

void print_gc_sections(const Linker &l, Out &out)
{
    auto line = [&](Str file, Str name) {
        out.put("removing unused section ").put(file).put(":(").put(name).put(")\n");
    };
    for (u32 f : l.objects) {
        const InputFile &in = *l.files[f];
        const Object &o     = in.obj;
        Str name            = o.name.str();
        for (u32 k = 0; k < o.functions.size(); k++)
            if (!in.live_functions[k])
                line(name, o.functions[k].name);
        for (u32 k = 0; k < o.segments.size(); k++)
            if (!in.live_segments[k])
                line(name, o.segments[k].name);
        for (u32 k = 0; k < o.globals.size(); k++)
            if (!in.live_globals[k])
                line(name, o.globals[k].name);
        for (u32 k = 0; k < o.tables.size(); k++)
            if (!in.live_tables[k])
                line(name, o.tables[k].name);
    }
    for (const Sym &g : l.syms)
        if (!g.live && (g.stub || (g.synthetic() && g.kind == SYM_FUNCTION)))
            line("<internal>", g.name);
    for (const Sym &g : l.syms)
        if (!g.live && g.synthetic() && g.state == State::Defined && g.kind == SYM_GLOBAL)
            line("<internal>", g.name);
}
