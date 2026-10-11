#include "resolve.h"

#include "kernel/hash.h"
#include "optable.h"
#include "out.h"

using namespace wat;

namespace {

// One index space: ids to numbers, and how many there are.
struct Ids {
    Str what; // for messages
    HashMap<Str, u32> ids;
    u32 count = 0;
};

// One sequence of instructions under way.
struct Frame {
    Instr *owner; // the instruction whose body it is; null at the top
    u32 part;     // the owner's part that follows it
    List<Instr *> seq;
    u32 next;
};

bool is_idchar(u8 c)
{
    if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
        return true;
    Str more = "!#$%&'*+-./:<=>?@\\^_`|~";
    return more.find(char(c)) != Str::npos;
}

bool same(const RefType &a, const RefType &b)
{
    if (a.nullable != b.nullable || a.heap.kind != b.heap.kind)
        return false;
    if (a.heap.kind == HeapType::Kind::Abstract)
        return a.heap.abs == b.heap.abs;
    return a.heap.index.value == b.heap.index.value;
}

bool same(const ValType &a, const ValType &b)
{
    return a.kind == b.kind && (a.kind != ValType::Kind::Ref || same(a.type, b.type));
}

// A use's signature is function type `f`'s.
bool same(const TypeUse &u, const CompType &f)
{
    if (u.params.size() != f.params.size() || u.results.size() != f.results.size())
        return false;
    for (u32 i = 0; i < u.params.size(); i++)
        if (!same(u.params[i].type, f.params[i].type))
            return false;
    for (u32 i = 0; i < u.results.size(); i++)
        if (!same(u.results[i], f.results[i]))
            return false;
    return true;
}

struct Resolver {
    Str file;
    Arena &arena;
    Diag &diag;
    bool failed = false;

    Ids types, funcs, tables, memories, globals, tags, elems, datas;
    Vec<TypeDef *> defs; // every type, by index
    Vec<bool> alone;     // the type is a rec group of its own

    HashMap<Str, u32> locals;
    Vec<Opt<Str>> labels; // innermost last
    Vec<Frame> stack;

    Vec<Decl::Data *> segments; // every data segment, null where inline
    Vec<u64> address;           // of each, where it has a Sym

    Vec<Decl *> out;
    Vec<Decl *> implicit;
    u32 defined[5] = {}; // functions, tables, memories, globals, tags so far

    Resolver(Str name, Arena &a, Diag &d) : file(name), arena(a), diag(d)
    {
        types.what    = "type";
        funcs.what    = "function";
        tables.what   = "table";
        memories.what = "memory";
        globals.what  = "global";
        tags.what     = "tag";
        elems.what    = "elem segment";
        datas.what    = "data segment";
    }

    // ---------------------------------------------------- errors

    void fail(Loc at, Str msg)
    {
        if (failed)
            return;
        failed = true;
        Out where;
        where.put(file).put(':').num(at.line).put(':').num(at.col);
        diag.error_at(where.str(), msg);
    }

    void oom()
    {
        if (!failed)
            diag.error("out of memory");
        failed = true;
    }

    // "what $id"
    void named(Loc at, Str what, Str id)
    {
        Out m;
        m.put(what).put(" $");
        bool plain = !id.empty();
        for (char c : id)
            plain = plain && is_idchar(u8(c));
        if (plain) {
            m.put(id);
        } else {
            m.put('"');
            for (char ch : id) {
                u8 c = u8(ch);
                if (c < 0x20 || c == 0x7f || c == '"' || c == '\\')
                    m.put('\\').hex(c, 2);
                else
                    m.put(ch);
            }
            m.put('"');
        }
        fail(at, m.str());
    }

    void unknown(const Idx &x, Str what)
    {
        Out m;
        m.put("unknown ").put(what);
        named(x.loc, m.str(), x.id);
    }

    // "what N"
    void numbered(Loc at, Str what, u32 n)
    {
        Out m;
        m.put(what).put(' ').num(n);
        fail(at, m.str());
    }

    template <class T>
    void add(Vec<T> &v, T x)
    {
        if (!v.push(move(x)))
            oom();
    }

    template <class T>
    T *node(Loc at)
    {
        T *n = arena.node<T>(at);
        if (!n)
            oom();
        return n;
    }

    // ---------------------------------------------------- names

    void bind(const Bind &b, Ids &s)
    {
        u32 i = s.count++;
        if (!b.id.has || failed)
            return;
        if (s.ids.contains(b.id.value)) {
            Out m;
            m.put("duplicate ").put(s.what);
            named(b.loc, m.str(), b.id.value);
        } else if (!s.ids.insert(b.id.value, i)) {
            oom();
        }
    }

    // Params and locals share `locals`; fields use it too.
    void bind_local(const Bind &b, u32 i, Str what)
    {
        if (!b.id.has || failed)
            return;
        if (locals.contains(b.id.value))
            named(b.loc, what, b.id.value);
        else if (!locals.insert(b.id.value, i))
            oom();
    }

    void index(Idx &x, Ids &s)
    {
        if (x.kind != Idx::Kind::Id || failed)
            return;
        const u32 *v = s.ids.find(x.id);
        if (!v) {
            unknown(x, s.what);
            return;
        }
        x.kind  = Idx::Kind::Num;
        x.value = *v;
    }

    void local(Idx &x)
    {
        if (x.kind != Idx::Kind::Id || failed)
            return;
        const u32 *v = locals.find(x.id);
        if (!v) {
            unknown(x, "local");
            return;
        }
        x.kind  = Idx::Kind::Num;
        x.value = *v;
    }

    void label(Idx &x)
    {
        if (x.kind != Idx::Kind::Id || failed)
            return;
        for (u32 i = labels.size(); i-- > 0;)
            if (labels[i].has && labels[i].value == x.id) {
                x.kind  = Idx::Kind::Num;
                x.value = u32(labels.size() - 1 - i);
                return;
            }
        unknown(x, "label");
    }

    // Field `y` of type `x`, x resolved.
    void field(Idx &y, const Idx &x)
    {
        if (y.kind != Idx::Kind::Id || failed)
            return;
        if (x.kind == Idx::Kind::Num && x.value < defs.size()) {
            const CompType &c = defs[x.value]->type.body;
            if (c.kind == CompType::Kind::StructType)
                for (u32 i = 0; i < c.fields.size(); i++)
                    if (c.fields[i].bind.id.has && c.fields[i].bind.id.value == y.id) {
                        y.kind  = Idx::Kind::Num;
                        y.value = i;
                        return;
                    }
        }
        unknown(y, "field");
    }

    void idx(Idx &x, Space s)
    {
        switch (s) {
        case Space::FUNC:
            index(x, funcs);
            break;
        case Space::LOCAL:
            local(x);
            break;
        case Space::GLOBAL:
            index(x, globals);
            break;
        case Space::TABLE:
            index(x, tables);
            break;
        case Space::MEMORY:
            index(x, memories);
            break;
        case Space::TYPE:
            index(x, types);
            break;
        case Space::TAG:
            index(x, tags);
            break;
        case Space::ELEM:
            index(x, elems);
            break;
        case Space::DATA:
            index(x, datas);
            break;
        case Space::LABEL:
            label(x);
            break;
        default:
            break;
        }
    }

    Ids &space(ExternKind k)
    {
        switch (k) {
        case ExternKind::FuncKind:
            return funcs;
        case ExternKind::TableKind:
            return tables;
        case ExternKind::MemoryKind:
            return memories;
        case ExternKind::GlobalKind:
            return globals;
        default:
            return tags;
        }
    }

    // ---------------------------------------------------- types

    void heap(HeapType &h)
    {
        if (h.kind == HeapType::Kind::Concrete)
            index(h.index, types);
    }

    void ref(RefType &r) { heap(r.heap); }

    void val(ValType &t)
    {
        if (t.kind == ValType::Kind::Ref)
            ref(t.type);
    }

    void vals(List<ValType> &l)
    {
        for (ValType &t : l)
            val(t);
    }

    void storage(StorageType &s)
    {
        if (s.kind == StorageType::Kind::Value)
            val(s.type);
    }

    void type_def(TypeDef &t)
    {
        for (Idx &x : t.type.supers)
            index(x, types);
        CompType &c = t.type.body;
        for (Param &p : c.params)
            val(p.type);
        vals(c.results);
        for (Field &f : c.fields)
            storage(f.type.storage);
        storage(c.field.storage);
    }

    // Type `i` when it is a function type.
    const CompType *func_type(u32 i)
    {
        if (i >= defs.size() || defs[i]->type.body.kind != CompType::Kind::FuncType)
            return nullptr;
        return &defs[i]->type.body;
    }

    // "unknown type N" or "non-function type N", when type `x` is not a
    // function type.
    bool want_func(const Idx &x)
    {
        if (func_type(x.value))
            return true;
        numbered(x.loc, x.value < defs.size() ? "non-function type"_s : "unknown type"_s, x.value);
        return false;
    }

    // `l` without ids or names.
    List<Param> bare(List<Param> l)
    {
        bool named = false;
        for (const Param &p : l)
            named = named || p.bind.id.has || p.bind.name.has;
        if (!named)
            return l;
        Vec<Param> v;
        for (const Param &p : l) {
            Param q;
            q.type = p.type;
            add(v, q);
        }
        List<Param> r = arena.list(v);
        if (arena.failed())
            oom();
        return r;
    }

    // The first type that is `(type (func …))` of this signature, or a new
    // one at the end (§5.4).
    u32 implied(const TypeUse &u, Loc at)
    {
        for (u32 i = 0; i < defs.size(); i++) {
            const SubType &s = defs[i]->type;
            if (alone[i] && s.final && s.supers.empty() &&
                s.body.kind == CompType::Kind::FuncType && same(u, s.body))
                return i;
        }
        auto *d = node<Decl::Type>(at);
        auto *t = arena.make<TypeDef>();
        if (!d || !t) {
            oom();
            return 0;
        }
        t->type.body.params  = bare(u.params);
        t->type.body.results = u.results;
        d->group.p           = t;
        d->group.n           = 1;
        add(implicit, static_cast<Decl *>(d));
        add(defs, t);
        add(alone, true);
        return types.count++;
    }

    // A type use: its index, and the signature of the type it names.
    void type_use(TypeUse &u, Loc at)
    {
        for (Param &p : u.params)
            val(p.type);
        vals(u.results);
        if (failed)
            return;
        if (!u.type.has) {
            u32 i              = implied(u, at);
            u.type.has         = true;
            u.type.value       = Idx();
            u.type.value.value = i;
            u.type.value.loc   = at;
            return;
        }
        Idx &x = u.type.value;
        index(x, types);
        if (failed)
            return;
        if (!u.params.empty() || !u.results.empty()) {
            if (want_func(x) && !same(u, *func_type(x.value)))
                fail(x.loc, "inline function type does not match explicit type");
            return;
        }
        if (const CompType *f = func_type(x.value)) {
            u.params  = bare(f->params);
            u.results = f->results;
        }
    }

    void block_type(BlockType &b, Loc at)
    {
        if (b.kind == BlockType::Kind::Result)
            val(b.result);
        else if (b.kind == BlockType::Kind::Use)
            type_use(b.use, at);
    }

    // ---------------------------------------------------- instructions

    // A sequence, and every one nested in it. Not recursive, since blocks
    // nest without bound: a block's body is a frame on `stack`.
    void body(List<Instr *> seq)
    {
        stack.clear();
        enter(nullptr, 0, seq);
        while (!failed && !stack.empty()) {
            Frame &f = stack.back();
            List<Instr *> b;
            if (f.next < f.seq.size()) {
                Instr *in = f.seq[f.next++];
                if (part(in, 0, b))
                    enter(in, 1, b);
                continue;
            }
            Instr *owner = f.owner;
            u32 next     = f.part;
            stack.pop();
            if (owner && part(owner, next, b))
                enter(owner, next + 1, b);
        }
    }

    // A constant expression: no locals, no labels.
    void expr(List<Instr *> seq)
    {
        locals.clear();
        labels.clear();
        body(seq);
    }

    void enter(Instr *owner, u32 part, List<Instr *> seq)
    {
        if (!stack.push({ owner, part, seq, 0 }))
            oom();
    }

    void push(const Bind &label) { add(labels, label.id); }

    void pop() { labels.pop(); }

    // Part `k` of `in`: true, with `body` set, when a sequence comes next;
    // false once the instruction is done.
    bool part(Instr *in, u32 k, List<Instr *> &body)
    {
        switch (in->kind) {
        case Instr::Kind::Block: {
            auto *b = in->as<Instr::Block>();
            if (k == 0) {
                block_type(b->type, in->loc);
                push(b->label);
                body = b->body;
                return true;
            }
            pop();
            return false;
        }
        case Instr::Kind::If: {
            auto *b = in->as<Instr::If>();
            if (k == 0) {
                block_type(b->type, in->loc);
                push(b->label);
                body = b->then_body;
                return true;
            }
            if (k == 1) {
                body = b->else_body;
                return true;
            }
            pop();
            return false;
        }
        case Instr::Kind::TryTable: {
            // The catches are outside the block.
            auto *b = in->as<Instr::TryTable>();
            if (k == 0) {
                block_type(b->type, in->loc);
                for (Catch &c : b->catches) {
                    if (c.kind == Catch::Kind::Catch || c.kind == Catch::Kind::CatchRef)
                        index(c.tag, tags);
                    label(c.label);
                }
                push(b->label);
                body = b->body;
                return true;
            }
            pop();
            return false;
        }
        case Instr::Kind::Try: {
            auto *b = in->as<Instr::Try>();
            if (k == 0) {
                block_type(b->type, in->loc);
                push(b->label);
                body = b->body;
                return true;
            }
            if (k - 1 < b->catches.size()) {
                LegacyCatch &c = b->catches[k - 1];
                if (c.kind == LegacyCatch::Kind::LegacyCatch)
                    index(c.tag, tags);
                body = c.body;
                return true;
            }
            pop();
            return false;
        }
        case Instr::Kind::TryDelegate: {
            // Delegate counts from outside the try.
            auto *b = in->as<Instr::TryDelegate>();
            if (k == 0) {
                block_type(b->type, in->loc);
                push(b->label);
                body = b->body;
                return true;
            }
            pop();
            label(b->label_out);
            return false;
        }
        default:
            plain(in);
            return false;
        }
    }

    // An instruction without a body.
    void plain(Instr *in)
    {
        switch (in->kind) {
        case Instr::Kind::Select:
            vals(in->as<Instr::Select>()->results);
            break;
        case Instr::Kind::Br:
            label(in->as<Instr::Br>()->label);
            break;
        case Instr::Kind::BrTable: {
            auto *b = in->as<Instr::BrTable>();
            for (Idx &x : b->labels)
                label(x);
            label(b->default_);
            break;
        }
        case Instr::Kind::BrOnCast: {
            auto *b = in->as<Instr::BrOnCast>();
            label(b->label);
            ref(b->from);
            ref(b->to);
            break;
        }
        case Instr::Kind::Index: {
            auto *x = in->as<Instr::Index>();
            idx(x->x, op_def(x->op).x);
            break;
        }
        case Instr::Kind::Index2: {
            auto *x         = in->as<Instr::Index2>();
            const OpDef &op = op_def(x->op);
            idx(x->x, op.x);
            if (op.y == Space::FIELD)
                field(x->y, x->x);
            else
                idx(x->y, op.y);
            break;
        }
        case Instr::Kind::ArrayNewFixed:
            index(in->as<Instr::ArrayNewFixed>()->type, types);
            break;
        case Instr::Kind::CallIndirect: {
            auto *c = in->as<Instr::CallIndirect>();
            index(c->table, tables);
            type_use(c->type, in->loc);
            break;
        }
        case Instr::Kind::MemArg: {
            auto *m = in->as<Instr::MemArg>();
            index(m->memory, memories);
            if (m->addr.has)
                address_of(m->addr.value, m->offset);
            break;
        }
        case Instr::Kind::I32Const: {
            auto *c = in->as<Instr::I32Const>();
            u64 v   = 0;
            if (c->addr.has && address_of(c->addr.value, v))
                c->value = u32(v);
            break;
        }
        case Instr::Kind::I64Const: {
            auto *c = in->as<Instr::I64Const>();
            if (c->addr.has)
                address_of(c->addr.value, c->value);
            break;
        }
        case Instr::Kind::MemArgLane:
            index(in->as<Instr::MemArgLane>()->memory, memories);
            break;
        case Instr::Kind::RefNull:
            heap(in->as<Instr::RefNull>()->type);
            break;
        case Instr::Kind::RefTypeOp:
            ref(in->as<Instr::RefTypeOp>()->type);
            break;
        default:
            break;
        }
    }

    // ---------------------------------------------------- data annotations

    // What `a` stands for: its segment's address plus its addend.
    bool address_of(Addr &a, u64 &v)
    {
        Idx x = a.data;
        index(a.data, datas);
        if (failed)
            return false;
        u32 k = a.data.value;
        if (k >= segments.size() || !segments[k] || !segments[k]->sym.has) {
            named(x.loc, "@reloc annotation: no @sym on data segment", x.id);
            return false;
        }
        v = address[k] + a.addend;
        return true;
    }

    // Memory 0's address type; false with none.
    bool memory0(List<Decl *> decls, AddrType &a)
    {
        for (Decl *d : decls) {
            if (auto *i = d->as<Decl::Import>(); i && i->desc.kind == Extern::Kind::MemoryImport) {
                a = i->desc.memory.addr;
                return true;
            }
            if (auto *m = d->as<Decl::Memory>()) {
                a = m->type.addr;
                return true;
            }
        }
        return false;
    }

    // Every segment with a Sym active in memory 0, where the text put it or
    // at the next address its alignment allows; then the addresses in the
    // segments' bytes.
    void layout(List<Decl *> decls)
    {
        if (!address.resize(segments.size())) {
            oom();
            return;
        }
        AddrType at = AddrType::Addr32;
        bool memory = false;
        u64 next    = 0;
        for (u32 k = 0; k < segments.size() && !failed; k++) {
            Decl::Data *d = segments[k];
            if (!d || !d->sym.has)
                continue;
            if (!memory && !memory0(decls, at)) {
                fail(d->loc, "@sym annotation: no memory");
                return;
            }
            memory = true;
            if (!d->bind.id.has) {
                fail(d->loc, "@sym annotation: data segment without an id");
                return;
            }
            if (d->sym.value.section == SymSection::SBss)
                for (char c : d->init)
                    if (c) {
                        fail(d->loc, "@sym annotation: bss data not zero");
                        return;
                    }
            DataMode &m = d->mode;
            if (m.kind == DataMode::Kind::DataPassive) {
                u64 align  = d->sym.value.align;
                next       = (next + align - 1) & ~(align - 1);
                m.kind     = DataMode::Kind::DataActive;
                m.memory   = num(0, d->loc);
                m.offset   = zero(at, d->loc);
                address[k] = next;
                Instr *c   = m.offset[0];
                if (auto *c32 = c->as<Instr::I32Const>())
                    c32->value = u32(next);
                else
                    c->as<Instr::I64Const>()->value = next;
            } else {
                index(m.memory, memories);
                auto *c32 = m.offset.size() == 1 ? m.offset[0]->as<Instr::I32Const>() : nullptr;
                auto *c64 = m.offset.size() == 1 ? m.offset[0]->as<Instr::I64Const>() : nullptr;
                if (failed)
                    return;
                if (m.memory.value) {
                    fail(d->loc, "@sym annotation: not in memory 0");
                    return;
                }
                if ((!c32 || c32->addr.has) && (!c64 || c64->addr.has)) {
                    fail(d->loc, "@sym annotation: offset not a constant");
                    return;
                }
                address[k] = c32 ? c32->value : c64->value;
            }
            next = address[k] + d->init.size();
        }
        for (Decl::Data *d : segments) {
            if (!d || d->addrs.empty() || failed)
                continue;
            if (at == AddrType::Addr64) {
                fail(d->loc, "@reloc annotation: a 64-bit address in data");
                return;
            }
            Str copy = arena.str(d->init);
            if (arena.failed()) {
                oom();
                return;
            }
            char *b = const_cast<char *>(copy.data());
            for (DataAddr &a : d->addrs) {
                u64 v = 0;
                if (!address_of(a.addr, v))
                    return;
                for (u32 i = 0; i < 4; i++)
                    b[a.at + i] = char(v >> (8 * i));
            }
            d->init = copy;
        }
    }

    // ---------------------------------------------------- module fields

    // Every definition's id, in order: imports come first, as the parser
    // saw to.
    void bind_all(List<Decl *> decls)
    {
        for (Decl *d : decls) {
            if (failed)
                return;
            switch (d->kind) {
            case Decl::Kind::Type: {
                auto *t = d->as<Decl::Type>();
                for (TypeDef &def : t->group) {
                    bind(def.bind, types);
                    add(defs, &def);
                    add(alone, t->group.size() == 1);
                    locals.clear();
                    u32 i = 0;
                    for (const Field &f : def.type.body.fields)
                        bind_local(f.bind, i++, "duplicate field");
                }
                break;
            }
            case Decl::Kind::Import: {
                const Extern &e = d->as<Decl::Import>()->desc;
                bind(e.bind, space(ExternKind(u8(e.kind))));
                break;
            }
            case Decl::Kind::Func:
                bind(d->as<Decl::Func>()->bind, funcs);
                break;
            case Decl::Kind::Table: {
                auto *t = d->as<Decl::Table>();
                bind(t->bind, tables);
                elems.count += t->elems.has;
                break;
            }
            case Decl::Kind::Memory: {
                auto *m = d->as<Decl::Memory>();
                bind(m->bind, memories);
                datas.count += m->data.has;
                if (m->data.has)
                    add(segments, static_cast<Decl::Data *>(nullptr));
                break;
            }
            case Decl::Kind::Global:
                bind(d->as<Decl::Global>()->bind, globals);
                break;
            case Decl::Kind::Tag:
                bind(d->as<Decl::Tag>()->bind, tags);
                break;
            case Decl::Kind::Elem:
                bind(d->as<Decl::Elem>()->bind, elems);
                break;
            case Decl::Kind::Data:
                bind(d->as<Decl::Data>()->bind, datas);
                add(segments, d->as<Decl::Data>());
                break;
            default:
                break;
            }
        }
    }

    Idx num(u32 v, Loc at)
    {
        Idx x;
        x.value = v;
        x.loc   = at;
        return x;
    }

    // Inline exports, as Exports after their definition.
    void exports(List<Str> &names, ExternKind sort, u32 index, Loc at)
    {
        for (Str n : names) {
            auto *e = node<Decl::Export>(at);
            if (!e)
                return;
            e->name  = n;
            e->sort  = sort;
            e->index = num(index, at);
            add(out, static_cast<Decl *>(e));
        }
        names = List<Str>();
    }

    // An inline segment's offset: 0 of the address type.
    List<Instr *> zero(AddrType a, Loc at)
    {
        Vec<Instr *> v;
        if (a == AddrType::Addr64) {
            if (auto *c = node<Instr::I64Const>(at))
                add(v, static_cast<Instr *>(c));
        } else if (auto *c = node<Instr::I32Const>(at)) {
            add(v, static_cast<Instr *>(c));
        }
        List<Instr *> l = arena.list(v);
        if (arena.failed())
            oom();
        return l;
    }

    // `func x*` as the table's type and `(ref.func x)*`.
    void ref_funcs(ElemList &l, const RefType &type)
    {
        Opcode op = 0;
        find_op("ref.func", op);
        Vec<Expr> items;
        for (const Idx &x : l.funcs) {
            auto *r = node<Instr::Index>(x.loc);
            if (!r)
                return;
            r->op = op;
            r->x  = x;
            Vec<Instr *> v;
            add(v, static_cast<Instr *>(r));
            Expr e;
            e.instrs = arena.list(v);
            add(items, e);
        }
        l.kind  = ElemList::Kind::Exprs;
        l.type  = type;
        l.funcs = List<Idx>();
        l.items = arena.list(items);
        if (arena.failed())
            oom();
    }

    void elem_list(ElemList &l)
    {
        if (l.kind == ElemList::Kind::Funcs) {
            for (Idx &x : l.funcs)
                index(x, funcs);
            return;
        }
        ref(l.type);
        for (Expr &e : l.items)
            expr(e.instrs);
    }

    void func(Decl::Func *f)
    {
        TypeUse &u   = f->type;
        bool written = !u.params.empty() || !u.results.empty();
        type_use(u, f->loc);
        if (failed)
            return;
        // Named locals count from the type's params.
        if (!written && !func_type(u.type.value.value))
            for (const Local &l : f->locals)
                if (l.bind.id.has) {
                    want_func(u.type.value);
                    return;
                }
        locals.clear();
        u32 n = 0;
        for (const Param &p : u.params)
            bind_local(p.bind, n++, "duplicate local");
        for (Local &l : f->locals) {
            val(l.type);
            bind_local(l.bind, n++, "duplicate local");
        }
        labels.clear();
        body(f->body);
    }

    void decl(Decl *d)
    {
        switch (d->kind) {
        case Decl::Kind::Type:
            add(out, d);
            break;
        case Decl::Kind::Import: {
            auto *i   = d->as<Decl::Import>();
            Extern &e = i->desc;
            auto sort = ExternKind(u8(e.kind));
            if (e.kind == Extern::Kind::FuncImport || e.kind == Extern::Kind::TagImport)
                type_use(e.type, d->loc);
            else if (e.kind == Extern::Kind::TableImport)
                ref(e.table.elem);
            else if (e.kind == Extern::Kind::GlobalImport)
                val(e.global.type);
            add(out, d);
            exports(i->exports, sort, defined[u8(sort)]++, d->loc);
            break;
        }
        case Decl::Kind::Func: {
            auto *f = d->as<Decl::Func>();
            func(f);
            add(out, d);
            exports(f->exports, ExternKind::FuncKind, defined[0]++, d->loc);
            break;
        }
        case Decl::Kind::Table: {
            auto *t = d->as<Decl::Table>();
            u32 i   = defined[1]++;
            ref(t->type.elem);
            expr(t->init);
            add(out, d);
            if (t->elems.has) {
                auto *e = node<Decl::Elem>(d->loc);
                if (!e)
                    return;
                e->mode.kind   = ElemMode::Kind::ElemActive;
                e->mode.table  = num(i, d->loc);
                e->mode.offset = zero(t->type.addr, d->loc);
                e->elems       = t->elems.value;
                t->elems       = Opt<ElemList>();
                if (e->elems.kind == ElemList::Kind::Funcs)
                    ref_funcs(e->elems, t->type.elem);
                elem_list(e->elems);
                add(out, static_cast<Decl *>(e));
            }
            exports(t->exports, ExternKind::TableKind, i, d->loc);
            break;
        }
        case Decl::Kind::Memory: {
            auto *m = d->as<Decl::Memory>();
            u32 i   = defined[2]++;
            add(out, d);
            if (m->data.has) {
                auto *a = node<Decl::Data>(d->loc);
                if (!a)
                    return;
                a->mode.kind   = DataMode::Kind::DataActive;
                a->mode.memory = num(i, d->loc);
                a->mode.offset = zero(m->type.addr, d->loc);
                a->init        = m->data.value;
                m->data        = Opt<Str>();
                add(out, static_cast<Decl *>(a));
            }
            exports(m->exports, ExternKind::MemoryKind, i, d->loc);
            break;
        }
        case Decl::Kind::Global: {
            auto *g = d->as<Decl::Global>();
            val(g->type.type);
            expr(g->init);
            add(out, d);
            exports(g->exports, ExternKind::GlobalKind, defined[3]++, d->loc);
            break;
        }
        case Decl::Kind::Tag: {
            auto *t = d->as<Decl::Tag>();
            type_use(t->type, d->loc);
            add(out, d);
            exports(t->exports, ExternKind::TagKind, defined[4]++, d->loc);
            break;
        }
        case Decl::Kind::Export: {
            auto *e = d->as<Decl::Export>();
            index(e->index, space(e->sort));
            add(out, d);
            break;
        }
        case Decl::Kind::Start:
            index(d->as<Decl::Start>()->func, funcs);
            add(out, d);
            break;
        case Decl::Kind::Elem: {
            auto *e = d->as<Decl::Elem>();
            if (e->mode.kind == ElemMode::Kind::ElemActive) {
                index(e->mode.table, tables);
                expr(e->mode.offset);
            }
            elem_list(e->elems);
            add(out, d);
            break;
        }
        case Decl::Kind::Data: {
            auto *a = d->as<Decl::Data>();
            if (a->mode.kind == DataMode::Kind::DataActive) {
                index(a->mode.memory, memories);
                expr(a->mode.offset);
            }
            add(out, d);
            break;
        }
        case Decl::Kind::Custom:
            add(out, d);
            break;
        }
    }

    bool module(Module &m)
    {
        bind_all(m.decls);
        for (TypeDef *t : defs) {
            if (failed)
                break;
            type_def(*t);
        }
        if (!failed)
            layout(m.decls);
        for (Decl *d : m.decls) {
            if (failed)
                break;
            decl(d);
        }
        for (Decl *d : implicit)
            add(out, d);
        if (failed)
            return false;
        m.decls = arena.list(out);
        if (arena.failed())
            oom();
        return !failed;
    }
};

} // namespace

bool resolve(Str name, Arena &arena, Module &m, Diag &diag)
{
    Resolver r(name, arena, diag);
    return r.module(m);
}
