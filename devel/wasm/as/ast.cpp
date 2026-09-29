#include "ast.h"

#include "kernel/alloc.h"
#include "out.h"

namespace wat {

// ------------------------------------------------------------ the arena

namespace {

constexpr usize BLOCK = 64 * 1024;

} // namespace

Arena::~Arena()
{
    while (blocks) {
        Block *b = blocks;
        blocks   = b->next;
        heap_free(b);
    }
}

void *Arena::alloc(usize n, usize align)
{
    if (oom)
        return nullptr;
    usize pad = (align - usize(at) % align) % align;
    if (!at || n + pad > usize(end - at)) {
        usize head = (sizeof(Block) + 15) & ~usize(15);
        usize size = n + head + 16 > BLOCK ? n + head + 16 : BLOCK;
        auto *b    = static_cast<Block *>(heap_alloc(size));
        if (!b) {
            oom = true;
            return nullptr;
        }
        b->next = blocks;
        blocks  = b;
        at      = reinterpret_cast<u8 *>(b) + head;
        end     = reinterpret_cast<u8 *>(b) + size;
        pad     = (align - usize(at) % align) % align;
    }
    void *p = at + pad;
    at += pad + n;
    return p;
}

Str Arena::str(Str s)
{
    if (s.empty())
        return Str();
    char *p = static_cast<char *>(alloc(s.size(), 1));
    if (!p)
        return Str();
    for (usize i = 0; i < s.size(); i++)
        p[i] = s[i];
    return Str(p, s.size());
}

// ------------------------------------------------------------ the printer

// Every constructor with fields is (Name field …), each field in the order
// of wat.asdl; one without is Name, and a product is (Type field …). Bind
// is _, $id, @"name" or $id@"name"; Idx is a number or $id. An absent
// optional is _, a list is [a b], a float is its bits in hex, a string is
// quoted with \hh escapes. Decls and instructions are one to a line,
// indented by depth. With locations, @line:col follows the constructor's
// name, or the index.

namespace {

constexpr Str EXTERN_KIND[] = { "FuncKind", "TableKind", "MemoryKind", "GlobalKind", "TagKind" };

constexpr Str SECTION[] = { "TypeSec", "ImportSec", "FuncSec",     "TableSec", "MemorySec",
                            "TagSec",  "GlobalSec", "ExportSec",   "StartSec", "ElemSec",
                            "CodeSec", "DataSec",   "DataCountSec" };

constexpr Str ABS_HEAP_TYPE[] = { "HAny",  "HEq",     "HI31", "HStruct", "HArray",  "HNone",
                                  "HFunc", "HNoFunc", "HExn", "HNoExn",  "HExtern", "HNoExtern" };

constexpr Str ADDR_TYPE[] = { "Addr32", "Addr64" };

constexpr Str BRANCH_HINT[] = { "Unlikely", "Likely" };

constexpr Str VAL_TYPE[] = { "I32", "I64", "F32", "F64", "V128", "Ref" };

constexpr Str STORAGE_TYPE[] = { "Value", "I8", "I16" };

constexpr Str COMP_TYPE[] = { "FuncType", "StructType", "ArrayType" };

constexpr Str BLOCK_TYPE[] = { "NoResult", "Result", "Use" };

constexpr Str EXTERN[] = { "FuncImport", "TableImport", "MemoryImport", "GlobalImport",
                           "TagImport" };

constexpr Str ELEM_MODE[] = { "ElemPassive", "ElemActive", "ElemDeclare" };

constexpr Str ELEM_LIST[] = { "Funcs", "Exprs" };

constexpr Str DATA_MODE[] = { "DataPassive", "DataActive" };

constexpr Str PLACE[] = { "BeforeFirst", "AfterLast", "Before", "After" };

constexpr Str CATCH[] = { "Catch", "CatchRef", "CatchAll", "CatchAllRef" };

constexpr Str LEGACY_CATCH[] = { "LegacyCatch", "LegacyCatchAll" };

constexpr Str DECL[] = { "Type", "Import", "Func",  "Table", "Memory", "Global",
                         "Tag",  "Export", "Start", "Elem",  "Data",   "Custom" };

constexpr Str INSTR[] = {
    "Plain",  "Select",     "Block",    "If",       "TryTable", "Try",           "TryDelegate",
    "Br",     "BrTable",    "BrOnCast", "Index",    "Index2",   "ArrayNewFixed", "CallIndirect",
    "MemArg", "MemArgLane", "I32Const", "I64Const", "F32Const", "F64Const",      "V128Const",
    "Lane",   "Shuffle",    "RefNull",  "RefTypeOp"
};

// A byte an $id may hold unquoted (§3 of the language).
bool is_idchar(u8 c)
{
    if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
        return true;
    Str more = "!#$%&'*+-./:<=>?@\\^_`|~";
    return more.find(char(c)) != Str::npos;
}

// A float's bits.
struct Bits {
    u64 v;
};

struct Printer {
    Out &out;
    Str (*op_name)(Opcode);
    bool locs;
    u32 depth;

    // One sequence of instructions under way.
    struct Frame {
        const Instr *owner; // the instruction whose body it is; null at the top
        u32 part;           // the owner's part that follows it
        List<Instr *> seq;
        u32 next;
    };

    Vec<Frame> stack;

    void put(Str s) { out.put(s); }

    void put(char c) { out.put(c); }

    void line()
    {
        put('\n');
        for (u32 i = 0; i < depth; i++)
            put("  ");
    }

    void loc(Loc l)
    {
        if (locs)
            out.put('@').num(l.line).put(':').num(l.col);
    }

    // "(Name", and the location when there is one.
    void open(Str name)
    {
        put('(');
        put(name);
    }

    void open(Str name, Loc l)
    {
        open(name);
        loc(l);
    }

    // " a b …"
    template <class... A>
    void fields(const A &...a)
    {
        ((put(' '), emit(a)), ...);
    }

    template <class... A>
    void node(Str name, const A &...a)
    {
        open(name);
        fields(a...);
        put(')');
    }

    // ---------------------------------------------------- primitives

    void emit(bool b)
    {
        if (b)
            put("true");
        else
            put("false");
    }

    void emit(u8 v) { out.num(v); }

    void emit(u32 v) { out.num(v); }

    void emit(u64 v)
    {
        char t[20];
        u32 k = 0;
        do {
            t[k++] = char('0' + v % 10);
            v /= 10;
        } while (v);
        while (k)
            put(t[--k]);
    }

    void emit(Bits b)
    {
        put("0x");
        if (b.v >> 32)
            out.hex(u32(b.v >> 32)).hex(u32(b.v), 8);
        else
            out.hex(u32(b.v));
    }

    void emit(Opcode op) { put(op_name(op)); }

    // A name or bytes.
    void emit(Str s)
    {
        put('"');
        for (char ch : s) {
            u8 c = u8(ch);
            if (c >= 0x20 && c < 0x7f && c != '"' && c != '\\')
                put(ch);
            else
                out.put('\\').hex(c, 2);
        }
        put('"');
    }

    void ident(Str s)
    {
        put('$');
        for (char c : s)
            if (!is_idchar(u8(c))) {
                emit(s);
                return;
            }
        put(s);
    }

    template <class T>
    void emit(const Opt<T> &o)
    {
        if (o.has)
            emit(o.value);
        else
            put('_');
    }

    template <class T>
    void emit(const List<T> &l)
    {
        put('[');
        for (u32 i = 0; i < l.size(); i++) {
            if (i)
                put(' ');
            emit(l[i]);
        }
        put(']');
    }

    void emit(ExternKind k) { put(EXTERN_KIND[u32(k)]); }

    void emit(Section s) { put(SECTION[u32(s)]); }

    void emit(AbsHeapType t) { put(ABS_HEAP_TYPE[u32(t)]); }

    void emit(AddrType a) { put(ADDR_TYPE[u32(a)]); }

    void emit(BranchHint h) { put(BRANCH_HINT[u32(h)]); }

    // ---------------------------------------------------- names and types

    void emit(const Bind &b)
    {
        if (!b.id.has && !b.name.has)
            put('_');
        if (b.id.has)
            ident(b.id.value);
        if (b.name.has) {
            put('@');
            emit(b.name.value);
        }
    }

    void emit(const Idx &x)
    {
        if (x.kind == Idx::Kind::Num)
            out.num(x.value);
        else
            ident(x.id);
        loc(x.loc);
    }

    void emit(const HeapType &h)
    {
        if (h.kind == HeapType::Kind::Abstract)
            node("Abstract", h.abs);
        else
            node("Concrete", h.index);
    }

    void emit(const RefType &r) { node("RefType", r.nullable, r.heap); }

    void emit(const ValType &t)
    {
        if (t.kind == ValType::Kind::Ref)
            node("Ref", t.type);
        else
            put(VAL_TYPE[u32(t.kind)]);
    }

    void emit(const StorageType &s)
    {
        if (s.kind == StorageType::Kind::Value)
            node("Value", s.type);
        else
            put(STORAGE_TYPE[u32(s.kind)]);
    }

    void emit(const FieldType &f) { node("FieldType", f.storage, f.mutable_); }

    void emit(const Param &p) { node("Param", p.bind, p.type); }

    void emit(const Field &f) { node("Field", f.bind, f.type); }

    void emit(const CompType &c)
    {
        open(COMP_TYPE[u32(c.kind)]);
        switch (c.kind) {
        case CompType::Kind::FuncType:
            fields(c.params, c.results);
            break;
        case CompType::Kind::StructType:
            fields(c.fields);
            break;
        case CompType::Kind::ArrayType:
            fields(c.field);
            break;
        }
        put(')');
    }

    void emit(const SubType &s) { node("SubType", s.final, s.supers, s.body); }

    void emit(const TypeDef &t) { node("TypeDef", t.bind, t.type); }

    void emit(const Limits &l) { node("Limits", l.min, l.max); }

    void emit(const MemType &m) { node("MemType", m.addr, m.limits, m.shared, m.page_size); }

    void emit(const TableType &t) { node("TableType", t.addr, t.limits, t.elem); }

    void emit(const GlobalType &g) { node("GlobalType", g.type, g.mutable_); }

    void emit(const TypeUse &u) { node("TypeUse", u.type, u.params, u.results); }

    void emit(const BlockType &b)
    {
        if (b.kind == BlockType::Kind::Result)
            node("Result", b.result);
        else if (b.kind == BlockType::Kind::Use)
            node("Use", b.use);
        else
            put(BLOCK_TYPE[u32(b.kind)]);
    }

    // ---------------------------------------------------- module parts

    void emit(const Extern &e)
    {
        open(EXTERN[u32(e.kind)]);
        fields(e.bind);
        switch (e.kind) {
        case Extern::Kind::FuncImport:
        case Extern::Kind::TagImport:
            fields(e.type);
            break;
        case Extern::Kind::TableImport:
            fields(e.table);
            break;
        case Extern::Kind::MemoryImport:
            fields(e.memory);
            break;
        case Extern::Kind::GlobalImport:
            fields(e.global);
            break;
        }
        put(')');
    }

    void emit(const Local &l) { node("Local", l.bind, l.type); }

    void emit(const ElemMode &m)
    {
        if (m.kind == ElemMode::Kind::ElemActive)
            node("ElemActive", m.table, m.offset);
        else
            put(ELEM_MODE[u32(m.kind)]);
    }

    void emit(const Expr &x) { node("Expr", x.instrs); }

    void emit(const ElemList &e)
    {
        if (e.kind == ElemList::Kind::Funcs)
            node(ELEM_LIST[u32(e.kind)], e.funcs);
        else
            node(ELEM_LIST[u32(e.kind)], e.type, e.items);
    }

    void emit(const DataMode &m)
    {
        if (m.kind == DataMode::Kind::DataActive)
            node("DataActive", m.memory, m.offset);
        else
            put(DATA_MODE[u32(m.kind)]);
    }

    void emit(const Place &p)
    {
        if (p.kind == Place::Kind::Before || p.kind == Place::Kind::After)
            node(PLACE[u32(p.kind)], p.section);
        else
            put(PLACE[u32(p.kind)]);
    }

    void emit(const Catch &c)
    {
        if (c.kind == Catch::Kind::Catch || c.kind == Catch::Kind::CatchRef)
            node(CATCH[u32(c.kind)], c.tag, c.label);
        else
            node(CATCH[u32(c.kind)], c.label);
    }

    // ---------------------------------------------------- instructions

    // [instr …], one to a line. Not recursive, since blocks nest without
    // bound: a block's body is a frame on `stack`.
    void emit(const List<Instr *> &seq)
    {
        put('[');
        enter(nullptr, 0, seq);
        while (!stack.empty()) {
            Frame &f = stack.back();
            List<Instr *> body;
            if (f.next < f.seq.size()) {
                const Instr *in = f.seq[f.next++];
                line();
                if (part(in, 0, body))
                    enter(in, 1, body);
                continue;
            }
            const Instr *owner = f.owner;
            u32 next           = f.part;
            stack.pop();
            depth--;
            put(']');
            if (owner && part(owner, next, body))
                enter(owner, next + 1, body);
        }
    }

    void enter(const Instr *owner, u32 part, List<Instr *> body)
    {
        if (owner)
            put('[');
        if (!stack.push({ owner, part, body, 0 })) {
            out.oom = true;
            stack.clear();
            return;
        }
        depth++;
    }

    // Part `k` of `in`: true, with `body` set, when an instruction sequence
    // comes next; false once the instruction is closed.
    bool part(const Instr *in, u32 k, List<Instr *> &body)
    {
        if (k == 0)
            open(INSTR[u32(in->kind)], in->loc);
        switch (in->kind) {
        case Instr::Kind::Block: {
            auto *b = in->as<Instr::Block>();
            if (k == 0) {
                fields(b->op, b->label, b->type);
                return next(body, b->body);
            }
            break;
        }
        case Instr::Kind::If: {
            auto *b = in->as<Instr::If>();
            if (k == 0) {
                fields(b->label, b->type);
                return next(body, b->then_body);
            }
            if (k == 1) {
                fields(b->has_else);
                return next(body, b->else_body);
            }
            fields(b->hint);
            break;
        }
        case Instr::Kind::TryTable: {
            auto *b = in->as<Instr::TryTable>();
            if (k == 0) {
                fields(b->label, b->type, b->catches);
                return next(body, b->body);
            }
            break;
        }
        case Instr::Kind::Try: {
            auto *b = in->as<Instr::Try>();
            if (k == 0) {
                fields(b->label, b->type);
                return next(body, b->body);
            }
            // [(LegacyCatch tag [...]) (LegacyCatchAll [...])], catch k-1 next
            u32 c = k - 1;
            if (c == 0)
                put(" [");
            else
                put(')');
            if (c < b->catches.size()) {
                const LegacyCatch &lc = b->catches[c];
                if (c)
                    put(' ');
                open(LEGACY_CATCH[u32(lc.kind)]);
                if (lc.kind == LegacyCatch::Kind::LegacyCatch)
                    fields(lc.tag);
                return next(body, lc.body);
            }
            put(']');
            break;
        }
        case Instr::Kind::TryDelegate: {
            auto *b = in->as<Instr::TryDelegate>();
            if (k == 0) {
                fields(b->label, b->type);
                return next(body, b->body);
            }
            fields(b->label_out);
            break;
        }
        default:
            plain(in);
            break;
        }
        put(')');
        return false;
    }

    bool next(List<Instr *> &body, const List<Instr *> &l)
    {
        put(' ');
        body = l;
        return true;
    }

    // The fields of an instruction without a body.
    void plain(const Instr *in)
    {
        switch (in->kind) {
        case Instr::Kind::Plain:
            fields(in->as<Instr::Plain>()->op);
            break;
        case Instr::Kind::Select: {
            auto *s = in->as<Instr::Select>();
            fields(s->typed, s->results);
            break;
        }
        case Instr::Kind::Br: {
            auto *b = in->as<Instr::Br>();
            fields(b->op, b->label, b->hint);
            break;
        }
        case Instr::Kind::BrTable: {
            auto *b = in->as<Instr::BrTable>();
            fields(b->labels, b->default_);
            break;
        }
        case Instr::Kind::BrOnCast: {
            auto *b = in->as<Instr::BrOnCast>();
            fields(b->op, b->label, b->from, b->to);
            break;
        }
        case Instr::Kind::Index: {
            auto *x = in->as<Instr::Index>();
            fields(x->op, x->x);
            break;
        }
        case Instr::Kind::Index2: {
            auto *x = in->as<Instr::Index2>();
            fields(x->op, x->x, x->y);
            break;
        }
        case Instr::Kind::ArrayNewFixed: {
            auto *a = in->as<Instr::ArrayNewFixed>();
            fields(a->type, a->count);
            break;
        }
        case Instr::Kind::CallIndirect: {
            auto *c = in->as<Instr::CallIndirect>();
            fields(c->op, c->table, c->type);
            break;
        }
        case Instr::Kind::MemArg: {
            auto *m = in->as<Instr::MemArg>();
            fields(m->op, m->memory, m->offset, m->align);
            break;
        }
        case Instr::Kind::MemArgLane: {
            auto *m = in->as<Instr::MemArgLane>();
            fields(m->op, m->memory, m->offset, m->align, m->lane);
            break;
        }
        case Instr::Kind::I32Const:
            fields(in->as<Instr::I32Const>()->value);
            break;
        case Instr::Kind::I64Const:
            fields(in->as<Instr::I64Const>()->value);
            break;
        case Instr::Kind::F32Const:
            fields(Bits{ in->as<Instr::F32Const>()->bits });
            break;
        case Instr::Kind::F64Const:
            fields(Bits{ in->as<Instr::F64Const>()->bits });
            break;
        case Instr::Kind::V128Const:
            fields(in->as<Instr::V128Const>()->value);
            break;
        case Instr::Kind::Lane: {
            auto *l = in->as<Instr::Lane>();
            fields(l->op, l->lane);
            break;
        }
        case Instr::Kind::Shuffle:
            fields(in->as<Instr::Shuffle>()->lanes);
            break;
        case Instr::Kind::RefNull:
            fields(in->as<Instr::RefNull>()->type);
            break;
        case Instr::Kind::RefTypeOp: {
            auto *r = in->as<Instr::RefTypeOp>();
            fields(r->op, r->type);
            break;
        }
        default:
            break;
        }
    }

    // ---------------------------------------------------- module fields

    void decl(const Decl *d)
    {
        open(DECL[u32(d->kind)], d->loc);
        switch (d->kind) {
        case Decl::Kind::Type:
            fields(d->as<Decl::Type>()->group);
            break;
        case Decl::Kind::Import: {
            auto *i = d->as<Decl::Import>();
            fields(i->module, i->item, i->desc, i->exports);
            break;
        }
        case Decl::Kind::Func: {
            auto *f = d->as<Decl::Func>();
            fields(f->bind, f->exports, f->type, f->locals, f->body);
            break;
        }
        case Decl::Kind::Table: {
            auto *t = d->as<Decl::Table>();
            fields(t->bind, t->exports, t->type, t->init, t->elems);
            break;
        }
        case Decl::Kind::Memory: {
            auto *m = d->as<Decl::Memory>();
            fields(m->bind, m->exports, m->type, m->data);
            break;
        }
        case Decl::Kind::Global: {
            auto *g = d->as<Decl::Global>();
            fields(g->bind, g->exports, g->type, g->init);
            break;
        }
        case Decl::Kind::Tag: {
            auto *t = d->as<Decl::Tag>();
            fields(t->bind, t->exports, t->type);
            break;
        }
        case Decl::Kind::Export: {
            auto *e = d->as<Decl::Export>();
            fields(e->name, e->sort, e->index);
            break;
        }
        case Decl::Kind::Start:
            fields(d->as<Decl::Start>()->func);
            break;
        case Decl::Kind::Elem: {
            auto *e = d->as<Decl::Elem>();
            fields(e->bind, e->mode, e->elems);
            break;
        }
        case Decl::Kind::Data: {
            auto *a = d->as<Decl::Data>();
            fields(a->bind, a->mode, a->init);
            break;
        }
        case Decl::Kind::Custom: {
            auto *c = d->as<Decl::Custom>();
            fields(c->name, c->place, c->content);
            break;
        }
        }
        put(')');
    }

    void module(const Module &m)
    {
        open("Module");
        fields(m.bind);
        put(" [");
        depth++;
        for (const Decl *d : m.decls) {
            line();
            decl(d);
        }
        depth--;
        put("])\n");
    }
};

} // namespace

void print(Out &out, const Module &m, Str (*op_name)(Opcode), bool locs)
{
    Printer p{ out, op_name, locs, 0, {} };
    p.module(m);
}

} // namespace wat
