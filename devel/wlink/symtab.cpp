#include "symtab.h"

#include "kernel/alloc.h"
#include "reader.h"

using namespace wasm;

Linker::~Linker()
{
    for (InputFile *f : files)
        heap_delete(f);
}

bool same_sig(const FuncType *a, const FuncType *b)
{
    auto eq = [](Bytes x, Bytes y) {
        if (x.size() != y.size())
            return false;
        for (usize i = 0; i < x.size(); i++)
            if (x[i] != y[i])
                return false;
        return true;
    };
    return eq(a->params, b->params) && eq(a->results, b->results);
}

Str valtype_name(u8 t)
{
    switch (t) {
    case I32:
        return "i32"_s;
    case I64:
        return "i64"_s;
    case F32:
        return "f32"_s;
    case F64:
        return "f64"_s;
    case V128:
        return "v128"_s;
    case FUNCREF:
        return "funcref"_s;
    case EXTERNREF:
        return "externref"_s;
    default:
        return "?"_s;
    }
}

// "(i32, i32) -> i32", as lld writes a signature.
void put_sig(Out &m, const FuncType *t)
{
    m.put('(');
    for (usize i = 0; i < t->params.size(); i++) {
        if (i)
            m.put(", ");
        m.put(valtype_name(t->params[i]));
    }
    m.put(") -> ");
    m.put(t->results.empty() ? Str("void") : valtype_name(t->results[0]));
}

namespace {

// One level of lld's recursion.
struct Frame {
    bool load; // loading a file; else registering an archive's members
    u32 file;  // load: the file; register: the first member
    u32 end;   // register: one past the last member
    u32 next;  // load: next symbol; register: next member
    u32 sub;   // register: next symbol of that member
    Str why;   // load: who wanted it, for --why-extract; empty if nobody
    u32 sym;   // load: the global symbol that pulled it
};

Str kind_name(u8 kind)
{
    switch (kind) {
    case SYM_FUNCTION:
        return "WASM_SYMBOL_TYPE_FUNCTION"_s;
    case SYM_DATA:
        return "WASM_SYMBOL_TYPE_DATA"_s;
    case SYM_GLOBAL:
        return "WASM_SYMBOL_TYPE_GLOBAL"_s;
    case SYM_TABLE:
        return "WASM_SYMBOL_TYPE_TABLE"_s;
    default:
        return "WASM_SYMBOL_TYPE_SECTION"_s;
    }
}

struct Resolver {
    Linker &l;
    Vec<Frame> stack;
    Str why; // who wants the member add_*() just asked for

    InputFile &file(u32 f) { return *l.files[f]; }

    u32 insert(Str name, bool &fresh)
    {
        if (u32 *id = l.names.find(name)) {
            fresh = false;
            return *id;
        }
        fresh  = true;
        u32 id = l.syms.size();
        Sym s{};
        s.name  = name;
        s.file  = NONE;
        s.index = NONE;
        if (!l.syms.push(s) || !l.names.insert(name, id)) {
            l.diag.error("out of memory");
            l.diag.stopped = true;
        }
        return id;
    }

    // ------------------------------------------------------------ errors

    void duplicate(const Sym &g, u32 f)
    {
        Out m;
        m.put("duplicate symbol: ").put(g.name);
        m.put("\n>>> defined in ").put(l.file_name(g.file));
        m.put("\n>>> defined in ").put(l.file_name(f));
        l.diag.error(m.str());
    }

    void type_mismatch(const Sym &g, u8 kind, u32 f)
    {
        Out m;
        m.put("symbol type mismatch: ").put(g.name);
        m.put("\n>>> defined as ").put(kind_name(g.kind)).put(" in ").put(l.file_name(g.file));
        m.put("\n>>> defined as ").put(kind_name(kind)).put(" in ").put(l.file_name(f));
        l.diag.error(m.str());
    }

    // wasm-ld warns and makes a stub that traps; wlink refuses.
    void sig_mismatch(const Sym &g, const FuncType *sig, u32 f)
    {
        Out m;
        m.put("function signature mismatch: ").put(g.name);
        m.put("\n>>> defined as ");
        put_sig(m, g.sig);
        m.put(" in ").put(l.file_name(g.file));
        m.put("\n>>> defined as ");
        put_sig(m, sig);
        m.put(" in ").put(l.file_name(f));
        l.diag.error(m.str());
    }

    void import_mismatch(const Sym &g, Str what, Str had, Str now, u32 f)
    {
        Out m;
        m.put("import ").put(what).put(" mismatch for symbol: ").put(g.name);
        m.put("\n>>> defined as ").put(had).put(" in ").put(l.file_name(g.file));
        m.put("\n>>> defined as ").put(now).put(" in ").put(l.file_name(f));
        l.diag.error(m.str());
    }

    // ------------------------------------------------------------ symbols

    // Whether a defined symbol lost its chunk to another file's comdat.
    bool discarded(const InputFile &f, const Symbol &s)
    {
        const Object &o = f.obj;
        u32 comdat      = NONE;
        if (s.kind == SYM_FUNCTION)
            comdat = o.functions[s.index - o.imported_functions].comdat;
        else if (s.kind == SYM_DATA && !(s.flags & SYM_ABSOLUTE))
            comdat = o.segments[s.segment].comdat;
        return comdat != NONE && !f.kept[comdat];
    }

    const FuncType *sig_of(const Object &o, const Symbol &s)
    {
        if (s.kind != SYM_FUNCTION)
            return nullptr;
        if (s.index < o.imported_functions)
            return &o.types[o.imports[s.import].type];
        return &o.types[o.functions[s.index - o.imported_functions].type];
    }

    void define(Sym &g, u32 f, u32 i)
    {
        const Object &o = file(f).obj;
        const Symbol &s = o.symbols[i];
        g.state         = State::Defined;
        g.kind          = s.kind;
        g.flags         = s.flags;
        g.file          = f;
        g.index         = i;
        g.sig           = sig_of(o, s);
        g.import_module = Str();
        g.import_name   = Str();
    }

    void add_defined(u32 f, u32 i)
    {
        const Object &o = file(f).obj;
        const Symbol &s = o.symbols[i];
        bool fresh;
        u32 id              = insert(s.name, fresh);
        file(f).symbols[i]  = id;
        Sym &g              = l.syms[id];
        const FuncType *sig = sig_of(o, s);
        if (fresh || g.state == State::Lazy)
            return define(g, f, i);
        if (g.kind != s.kind)
            return type_mismatch(g, s.kind, f);
        if (sig && g.sig && (g.state == State::Defined || g.called) && !same_sig(g.sig, sig))
            return sig_mismatch(g, sig, f);
        if (g.state != State::Defined)
            return define(g, f, i);
        if (s.weak())
            return;
        if (!g.weak())
            duplicate(g, f);
        define(g, f, i);
    }

    // A reference, or a definition its comdat discarded. Returns the member
    // to load for it, or NONE.
    u32 add_undefined(u32 f, u32 i)
    {
        const Object &o = file(f).obj;
        const Symbol &s = o.symbols[i];
        bool fresh;
        u32 id              = insert(s.name, fresh);
        file(f).symbols[i]  = id;
        Sym &g              = l.syms[id];
        const FuncType *sig = sig_of(o, s);
        bool called         = file(f).called[i];
        bool weak           = s.weak();
        Str name            = s.flags & SYM_EXPLICIT_NAME ? s.import_field : Str();

        if (fresh) {
            g.state         = State::Undefined;
            g.kind          = s.kind;
            g.flags         = s.flags;
            g.file          = f;
            g.index         = i;
            g.sig           = sig;
            g.called        = called;
            g.import_module = s.import_module;
            g.import_name   = name;
            return NONE;
        }
        if (g.state == State::Lazy) {
            if (weak) {
                g.flags |= SYM_WEAK;
                if (sig)
                    g.sig = sig;
                return NONE;
            }
            why = l.file_name(f);
            return pull(g);
        }
        if (g.kind != s.kind) {
            type_mismatch(g, s.kind, f);
            return NONE;
        }
        if (sig && !g.sig)
            g.sig = sig;
        if (sig && called && g.state == State::Defined && !same_sig(g.sig, sig)) {
            sig_mismatch(g, sig, f);
            return NONE;
        }
        if (g.state == State::Undefined) {
            if (!name.empty()) {
                if (g.import_name.empty())
                    g.import_name = name;
                else if (g.import_name != name)
                    import_mismatch(g, "name", g.import_name, name, f);
            }
            if (!s.import_module.empty()) {
                if (g.import_module.empty())
                    g.import_module = s.import_module;
                else if (g.import_module != s.import_module)
                    import_mismatch(g, "module", g.import_module, s.import_module, f);
            }
            if (called)
                g.called = true;
            if (g.weak() && !weak)
                g.flags &= ~SYM_BINDING_MASK;
        }
        return NONE;
    }

    // A member's definition, registered when its archive is added. Returns
    // the member when something is already waiting for it.
    u32 add_lazy(u32 m, u32 j)
    {
        const Symbol &s = file(m).obj.symbols[j];
        bool fresh;
        u32 id = insert(s.name, fresh);
        Sym &g = l.syms[id];
        if (!fresh && g.state != State::Undefined)
            return NONE;
        bool waiting        = !fresh && !g.weak();
        Str who             = fresh ? Str() : l.file_name(g.file);
        const FuncType *sig = g.sig;
        g.state             = State::Lazy;
        g.kind              = s.kind;
        g.flags             = fresh || waiting ? 0 : SYM_WEAK;
        g.file              = m;
        g.index             = j;
        g.sig               = fresh ? nullptr : sig;
        g.called            = false;
        g.import_module     = Str();
        g.import_name       = Str();
        if (!waiting)
            return NONE;
        why = who;
        return pull(g);
    }

    // The member behind a lazy symbol, unless it is loaded already: then its
    // definition was discarded, and the name stays undefined.
    u32 pull(Sym &g)
    {
        if (!file(g.file).loaded)
            return g.file;
        g.state = State::Undefined;
        return NONE;
    }

    // ------------------------------------------------------------ the stack

    void push_load(u32 f, Str why_ref, u32 sym)
    {
        Frame fr{};
        fr.load = true;
        fr.file = f;
        fr.why  = why_ref;
        fr.sym  = sym;
        stack.push(fr);
    }

    void start(u32 f)
    {
        InputFile &in = file(f);
        in.loaded     = true;
        in.lazy       = false;
        if (l.cfg.trace)
            l.out.put(in.obj.name.str()).put('\n');
        for (u32 c = 0; c < in.obj.comdats.size(); c++) {
            Str name  = in.obj.comdats[c].name;
            bool mine = !l.comdats.find(name);
            if (mine) {
                l.comdats.insert(name, f);
                l.comdat_order.push(name);
            }
            in.kept[c] = mine;
        }
    }

    // A file is an object once its load is done, after the members it pulled.
    void finish(const Frame &fr)
    {
        if (!l.objects.push(fr.file))
            l.diag.error("out of memory");
        if (fr.why.empty() || l.cfg.why_extract.empty())
            return;
        const Sym &g = l.syms[fr.sym];
        l.why.put(fr.why).put('\t').put(l.file_name(g.file)).put('\t').put(g.name).put('\n');
    }

    // One step of the top frame: a symbol added, or a member pushed.
    void step()
    {
        Frame &fr = stack.back();
        if (fr.load) {
            InputFile &in = file(fr.file);
            if (fr.next == 0 && !in.loaded)
                start(fr.file);
            while (fr.next < in.obj.symbols.size()) {
                u32 i           = fr.next++;
                const Symbol &s = in.obj.symbols[i];
                if (s.local())
                    continue;
                if (!s.undefined() && !discarded(in, s)) {
                    add_defined(fr.file, i);
                    continue;
                }
                u32 m = add_undefined(fr.file, i);
                if (m != NONE) {
                    push_load(m, why, file(fr.file).symbols[i]);
                    return;
                }
            }
            Frame done = fr;
            stack.pop();
            finish(done);
            return;
        }

        while (fr.next < fr.end) {
            InputFile &in = file(fr.next);
            if (in.loaded) {
                fr.next++;
                fr.sub = 0;
                continue;
            }
            while (fr.sub < in.obj.symbols.size()) {
                u32 j           = fr.sub++;
                const Symbol &s = in.obj.symbols[j];
                if (s.undefined() || s.local())
                    continue;
                u32 m = add_lazy(fr.next, j);
                if (m != NONE) {
                    u32 id = *l.names.find(s.name);
                    fr.next++;
                    fr.sub = 0;
                    push_load(m, why, id);
                    return;
                }
            }
            fr.next++;
            fr.sub = 0;
        }
        stack.pop();
    }

    bool drain()
    {
        while (!stack.empty() && !l.diag.stopped)
            step();
        return !l.diag.stopped;
    }

    // --entry, --export and -u: the linker's own references.
    void want(Str name, Str option)
    {
        u32 *id = l.names.find(name);
        if (!id || l.syms[*id].state != State::Lazy)
            return;
        u32 m = pull(l.syms[*id]);
        if (m != NONE)
            push_load(m, option, *id);
        drain();
    }

    void synthetic(Str name, u8 kind, u32 flags, const FuncType *sig)
    {
        bool fresh;
        Sym &g  = l.syms[insert(name, fresh)];
        g.state = State::Defined;
        g.kind  = kind;
        g.flags = flags;
        g.file  = NONE;
        g.index = NONE;
        g.sig   = sig;
    }

    // Defined by the linker only if something refers to them.
    void optional(Str name, u8 kind)
    {
        u32 *id = l.names.find(name);
        if (!id || l.syms[*id].state == State::Defined)
            return;
        Sym &g  = l.syms[*id];
        g.state = State::Defined;
        g.kind  = kind;
        g.flags = SYM_HIDDEN | (kind == SYM_DATA ? SYM_ABSOLUTE : 0) | (g.flags & SYM_NO_STRIP);
        g.file  = NONE;
        g.index = NONE;
    }

    // ------------------------------------------------------------ inputs

    bool add_file(Str name, Str member, Bytes bytes)
    {
        InputFile *f = heap_new<InputFile>();
        if (!f || !l.files.push(f)) {
            heap_delete(f);
            l.diag.error("out of memory");
            return false;
        }
        f->obj.file = bytes;
        f->obj.name.append(name);
        if (!member.empty()) {
            f->obj.name.push('(');
            f->obj.name.append(member);
            f->obj.name.push(')');
        }
        Out err;
        if (!read_object(f->obj, err)) {
            l.diag.error(err.str());
            return false;
        }
        const Object &o = f->obj;
        if (!f->symbols.resize(o.symbols.size()) || !f->called.resize(o.symbols.size()) ||
            !f->kept.resize(o.comdats.size())) {
            l.diag.error("out of memory");
            return false;
        }
        for (u32 &s : f->symbols)
            s = NONE;
        for (const RelocSection &rs : o.relocs)
            if (rs.target == o.code_section)
                for (const Reloc &r : rs.relocs)
                    if (r.type == R_FUNCTION_INDEX_LEB)
                        f->called[r.index] = 1;
        return true;
    }

    bool add_input(const Source &in)
    {
        if (!is_archive(in.bytes)) {
            u32 f = l.files.size();
            if (!add_file(in.name, Str(), in.bytes))
                return false;
            push_load(f, Str(), NONE);
            return drain();
        }
        Vec<Member> members;
        Out err;
        if (!read_archive(in.name, in.bytes, members, err)) {
            l.diag.error(err.str());
            return false;
        }
        u32 first = l.files.size();
        for (const Member &m : members) {
            if (!add_file(in.name, m.name, m.data))
                return false;
            l.files.back()->lazy   = true;
            l.files.back()->member = true;
        }
        Frame fr{};
        fr.file = first;
        fr.next = first;
        fr.end  = l.files.size();
        stack.push(fr);
        return drain();
    }

    bool run(Span<const Source> inputs)
    {
        static const FuncType NONE_TO_VOID{};
        synthetic("__wasm_call_ctors", SYM_FUNCTION, SYM_HIDDEN, &NONE_TO_VOID);
        synthetic("__stack_pointer", SYM_GLOBAL, 0, nullptr);

        for (const Source &in : inputs)
            if (!add_input(in))
                return false;

        for (Str name : l.cfg.undefined)
            want(name, "<internal>");
        for (Str name : l.cfg.exports)
            want(name, "--export");
        if (!l.cfg.entry.empty())
            want(l.cfg.entry, "--entry");
        if (l.diag.stopped)
            return false;

        for (Str name : l.cfg.exports) {
            u32 *id = l.names.find(name);
            if (!id || l.syms[*id].state != State::Defined) {
                Out m;
                m.put("symbol exported via --export not found: ").put(name);
                l.diag.error(m.str());
            }
        }
        if (!l.cfg.entry.empty()) {
            u32 *id = l.names.find(l.cfg.entry);
            if (!id || l.syms[*id].state != State::Defined) {
                Out m;
                m.put("entry symbol not defined (pass --no-entry to suppress): ");
                m.put(l.cfg.entry);
                l.diag.error(m.str());
            }
        }

        // __memory_base and __table_base are PIC's, and wasm-ld crashes on them.
        static const Str OPTIONAL[] = { "__dso_handle", "__data_end",    "__stack_low",
                                        "__stack_high", "__global_base", "__heap_base",
                                        "__heap_end" };
        for (Str name : OPTIONAL)
            optional(name, SYM_DATA);
        if (l.diag.failed())
            return false;

        // A weak undefined function gets a stub; one still lazy does not.
        for (Sym &g : l.syms)
            if (g.state == State::Undefined && g.weak() && g.kind == SYM_FUNCTION && g.sig)
                g.stub = true;

        // What is still lazy was only ever wanted weakly, or not at all.
        for (Sym &g : l.syms)
            if (g.state == State::Lazy && g.weak())
                g.state = State::Undefined;

        u32 *table = l.names.find("__indirect_function_table");
        if (table) {
            Sym &g = l.syms[*table];
            if (g.kind != SYM_TABLE) {
                l.diag.error("symbol __indirect_function_table is not a table");
                return false;
            }
            if (g.state == State::Defined) {
                l.diag.error("reserved symbol must not be defined: __indirect_function_table");
                return false;
            }
            optional("__indirect_function_table", SYM_TABLE);
        }
        return true;
    }
};

} // namespace

bool resolve(Linker &l, Span<const Source> inputs)
{
    Resolver *r = heap_new<Resolver>(l);
    if (!r) {
        l.diag.error("out of memory");
        return false;
    }
    bool ok = r->run(inputs);
    heap_delete(r);
    return ok && !l.diag.failed();
}

namespace {

bool allowed(const Linker &l, const Sym &g)
{
    return !g.import_name.empty() || l.cfg.allow_undefined;
}

} // namespace

bool imported(const Linker &l, const Sym &g)
{
    return g.kind != SYM_DATA && !g.weak() && allowed(l, g);
}

bool check_undefined(Linker &l)
{
    for (u32 f : l.objects) {
        const InputFile &in = *l.files[f];
        const Object &o     = in.obj;
        for (const RelocSection &rs : o.relocs) {
            u8 target = o.sections[rs.target].id;
            if (target != SEC_CODE && target != SEC_DATA)
                continue;
            const Vec<u8> &live = target == SEC_CODE ? in.live_functions : in.live_segments;
            for (const Reloc &r : rs.relocs) {
                if (reloc_symbol_kind(r.type) == SYM_NONE || !live[r.chunk])
                    continue;
                u32 id = in.symbols[r.index];
                if (id == NONE)
                    continue;
                const Sym &g = l.syms[id];
                if (g.state != State::Undefined || g.weak() || allowed(l, g))
                    continue;
                Out m;
                m.put(o.name.str()).put(": undefined symbol: ").put(g.name);
                l.diag.error(m.str());
                if (l.diag.stopped)
                    return false;
            }
        }
    }
    return !l.diag.failed();
}

void dump_symtab(const Linker &l, Out &out)
{
    static const Str KIND[] = { "function", "data", "global", "section", "tag", "table" };
    for (const Sym &g : l.syms) {
        Str state;
        switch (g.state) {
        case State::Defined:
            state = g.synthetic() ? "synthetic"_s : g.weak() ? "weak"_s : "defined"_s;
            break;
        case State::Undefined:
            state = imported(l, g) ? "import"_s : g.weak() ? "weak-undefined"_s : "undefined"_s;
            break;
        case State::Lazy:
            state = g.weak() ? "weak-lazy"_s : "lazy"_s;
            break;
        }
        out.put(state).put('\t').put(KIND[g.kind]).put('\t').put(g.name).put('\t');
        out.put(l.file_name(g.file));
        if (g.state == State::Undefined && imported(l, g)) {
            out.put('\t').put(g.import_module.empty() ? Str("env") : g.import_module).put('.');
            out.put(g.import_name.empty() ? g.name : g.import_name);
        }
        out.put('\n');
    }
    for (Str name : l.comdat_order)
        out.put("comdat\t").put(name).put('\t').put(l.file_name(*l.comdats.find(name))).put('\n');
}
