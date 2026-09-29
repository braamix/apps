#include "encode.h"

#include "emit.h"
#include "optable.h"
#include "wasm.h"

using namespace wat;

namespace {

constexpr u8 NUM_TYPE[] = { 0x7f, 0x7e, 0x7d, 0x7c, 0x7b };

constexpr u8 ABS_HEAP_TYPE[] = { 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x71,
                                 0x70, 0x73, 0x69, 0x74, 0x6f, 0x72 };

// Section ids.
enum : u8 {
    TYPE_SEC       = 1,
    IMPORT_SEC     = 2,
    FUNC_SEC       = 3,
    TABLE_SEC      = 4,
    MEMORY_SEC     = 5,
    GLOBAL_SEC     = 6,
    EXPORT_SEC     = 7,
    START_SEC      = 8,
    ELEM_SEC       = 9,
    CODE_SEC       = 10,
    DATA_SEC       = 11,
    DATA_COUNT_SEC = 12,
    TAG_SEC        = 13,
};

// Opcodes that are syntax, not instructions of the table.
enum : u8 { ELSE = 0x05, CATCH = 0x07, DELEGATE = 0x18, CATCH_ALL = 0x19, END = 0x0b };

u32 log2(u64 v)
{
    u32 n = 0;
    while (v > 1) {
        v >>= 1;
        n++;
    }
    return n;
}

struct Frame {
    const Instr *owner; // the instruction whose body it is; null at the top
    u32 part;           // the owner's part that follows it
    List<Instr *> seq;
    u32 next;
};

// A relocation: where it patches, first in the output and then in its
// section.
struct Reloc {
    u8 type;
    u8 id;       // the section's
    u32 section; // its index
    usize at;
    u32 index; // a symbol, or a type
};

// A symbol of the object: a function, table, global or tag.
struct Symbol {
    u8 kind;
    u32 flags;
    u32 index;
    Str name;
};

struct Encoder {
    Vec<u8> &v;
    Emit e;
    bool object;
    Vec<Frame> stack;
    Opcode fence = 0, ref_fn = 0, call_ref = 0, return_call_ref = 0;
    bool data_used = false; // code names a data segment

    // An object's relocations, and its symbols: functions, tables, globals
    // and tags, each kind in index order, from `first`.
    Vec<Reloc> relocs;
    usize sec_relocs  = 0; // the first of the section under way
    u32 sections      = 0; // written so far
    u32 count_section = 0; // the data count's
    Vec<Symbol> symbols;
    u32 first[4] = {};

    Encoder(Vec<u8> &out, bool obj) : v(out), e{ out }, object(obj)
    {
        find_op("atomic.fence", fence);
        find_op("ref.func", ref_fn);
        find_op("call_ref", call_ref);
        find_op("return_call_ref", return_call_ref);
    }

    // ---------------------------------------------------- relocations

    // An index an object relocates: a padded LEB, and its relocation.
    void reloc(u8 type, u32 value, u32 index)
    {
        if (!object) {
            e.uleb(value);
            return;
        }
        if (!relocs.push({ type, 0, 0, v.size(), index }))
            e.oom = true;
        e.uleb5(value);
    }

    void func_index(u32 x) { reloc(wasm::R_FUNCTION_INDEX_LEB, x, first[0] + x); }

    void table_index(u32 x) { reloc(wasm::R_TABLE_NUMBER_LEB, x, first[1] + x); }

    void global_index(u32 x) { reloc(wasm::R_GLOBAL_INDEX_LEB, x, first[2] + x); }

    void tag_index(u32 x) { reloc(wasm::R_TAG_INDEX_LEB, x, first[3] + x); }

    void type_index(u32 x) { reloc(wasm::R_TYPE_INDEX_LEB, x, x); }

    // ---------------------------------------------------- framing

    // The uleb of the size of what follows `start`, put before it; how
    // many bytes that took.
    u32 size(usize start)
    {
        u32 n = u32(v.size() - start);
        u8 b[5];
        u32 k = 0;
        do {
            b[k] = n & 0x7f;
            n >>= 7;
            if (n)
                b[k] |= 0x80;
            k++;
        } while (n);
        u32 n_bytes = k;
        while (k--)
            if (!v.insert(start, b[k]))
                e.oom = true;
        for (usize i = sec_relocs; i < relocs.size(); i++)
            if (relocs[i].at >= start)
                relocs[i].at += n_bytes;
        return n_bytes;
    }

    usize begin(u8 id)
    {
        e.byte(id);
        sec_relocs = relocs.size();
        return v.size();
    }

    // The section begun at `at` done: its size, and its relocations counted
    // from its contents.
    void end(u8 id, usize at)
    {
        usize payload = at + size(at);
        for (usize i = sec_relocs; i < relocs.size(); i++) {
            relocs[i].at -= payload;
            relocs[i].id      = id;
            relocs[i].section = sections;
        }
        sections++;
    }

    // ---------------------------------------------------- types

    void heap(const HeapType &h)
    {
        if (h.kind == HeapType::Kind::Abstract)
            e.byte(ABS_HEAP_TYPE[u8(h.abs)]);
        else
            e.sleb64(i64(h.index.value));
    }

    void ref(const RefType &r)
    {
        if (r.nullable && r.heap.kind == HeapType::Kind::Abstract) {
            heap(r.heap);
            return;
        }
        e.byte(r.nullable ? 0x63 : 0x64);
        heap(r.heap);
    }

    void val(const ValType &t)
    {
        if (t.kind == ValType::Kind::Ref)
            ref(t.type);
        else
            e.byte(NUM_TYPE[u8(t.kind)]);
    }

    void vals(const List<ValType> &l)
    {
        e.uleb(l.size());
        for (const ValType &t : l)
            val(t);
    }

    void storage(const StorageType &s)
    {
        if (s.kind == StorageType::Kind::I8)
            e.byte(0x78);
        else if (s.kind == StorageType::Kind::I16)
            e.byte(0x77);
        else
            val(s.type);
    }

    void field(const FieldType &f)
    {
        storage(f.storage);
        e.byte(f.mutable_);
    }

    void comp(const CompType &c)
    {
        switch (c.kind) {
        case CompType::Kind::FuncType:
            e.byte(0x60);
            e.uleb(c.params.size());
            for (const Param &p : c.params)
                val(p.type);
            vals(c.results);
            break;
        case CompType::Kind::StructType:
            e.byte(0x5f);
            e.uleb(c.fields.size());
            for (const Field &f : c.fields)
                field(f.type);
            break;
        case CompType::Kind::ArrayType:
            e.byte(0x5e);
            field(c.field);
            break;
        }
    }

    void sub(const SubType &s)
    {
        if (!s.final || !s.supers.empty()) {
            e.byte(s.final ? 0x4f : 0x50);
            e.uleb(s.supers.size());
            for (const Idx &x : s.supers)
                e.uleb(x.value);
        }
        comp(s.body);
    }

    void group(const List<TypeDef> &g)
    {
        if (g.size() == 1) {
            sub(g[0].type);
            return;
        }
        e.byte(0x4e);
        e.uleb(g.size());
        for (const TypeDef &t : g)
            sub(t.type);
    }

    void limits(const Limits &l, u8 flags)
    {
        e.byte(u8(flags | (l.max.has ? 1 : 0)));
        e.uleb64(l.min);
        if (l.max.has)
            e.uleb64(l.max.value);
    }

    void table_type(const TableType &t)
    {
        ref(t.elem);
        limits(t.limits, t.addr == AddrType::Addr64 ? 4 : 0);
    }

    void mem_type(const MemType &m)
    {
        bool page = m.page_size.has && m.page_size.value != 65536;
        u8 flags  = u8((m.shared ? 2 : 0) | (m.addr == AddrType::Addr64 ? 4 : 0) | (page ? 8 : 0));
        limits(m.limits, flags);
        if (page)
            e.uleb(log2(m.page_size.value));
    }

    void global_type(const GlobalType &g)
    {
        val(g.type);
        e.byte(g.mutable_);
    }

    void block_type(const BlockType &b)
    {
        if (b.kind == BlockType::Kind::Result) {
            val(b.result);
            return;
        }
        if (b.kind == BlockType::Kind::Use) {
            const TypeUse &u = b.use;
            if (!u.params.empty() || u.results.size() > 1) {
                if (object)
                    type_index(u.type.value.value);
                else
                    e.sleb64(i64(u.type.value.value));
                return;
            }
            if (u.results.size() == 1) {
                val(u.results[0]);
                return;
            }
        }
        e.byte(0x40);
    }

    // ---------------------------------------------------- instructions

    void op(Opcode o, u32 plus = 0)
    {
        const OpDef &d = op_def(o);
        if (d.prefix) {
            e.byte(d.prefix);
            e.uleb(d.sub + plus);
        } else {
            e.byte(u8(d.sub + plus));
        }
    }

    // A sequence, and every one nested in it, without its end. Not
    // recursive, since blocks nest without bound: a block's body is a
    // frame on `stack`.
    void seq(const List<Instr *> &l)
    {
        stack.clear();
        enter(nullptr, 0, l);
        while (!e.oom && !stack.empty()) {
            Frame &f = stack.back();
            List<Instr *> b;
            if (f.next < f.seq.size()) {
                const Instr *in = f.seq[f.next++];
                if (part(in, 0, b))
                    enter(in, 1, b);
                continue;
            }
            const Instr *owner = f.owner;
            u32 next           = f.part;
            stack.pop();
            if (owner && part(owner, next, b))
                enter(owner, next + 1, b);
        }
    }

    void enter(const Instr *owner, u32 part, List<Instr *> l)
    {
        if (!stack.push({ owner, part, l, 0 }))
            e.oom = true;
    }

    // A constant expression.
    void expr(const List<Instr *> &l)
    {
        seq(l);
        e.byte(END);
    }

    // Part `k` of `in`: true, with `body` set, when a sequence comes next;
    // false once the instruction is written.
    bool part(const Instr *in, u32 k, List<Instr *> &body)
    {
        switch (in->kind) {
        case Instr::Kind::Block: {
            auto *b = in->as<Instr::Block>();
            if (k == 0) {
                op(b->op);
                block_type(b->type);
                body = b->body;
                return true;
            }
            break;
        }
        case Instr::Kind::If: {
            // An empty else is left out.
            auto *b = in->as<Instr::If>();
            if (k == 0) {
                e.byte(0x04);
                block_type(b->type);
                body = b->then_body;
                return true;
            }
            if (k == 1 && !b->else_body.empty()) {
                e.byte(ELSE);
                body = b->else_body;
                return true;
            }
            break;
        }
        case Instr::Kind::TryTable: {
            auto *b = in->as<Instr::TryTable>();
            if (k == 0) {
                e.byte(0x1f);
                block_type(b->type);
                e.uleb(b->catches.size());
                for (const Catch &c : b->catches) {
                    e.byte(u8(c.kind));
                    if (c.kind == Catch::Kind::Catch || c.kind == Catch::Kind::CatchRef)
                        tag_index(c.tag.value);
                    e.uleb(c.label.value);
                }
                body = b->body;
                return true;
            }
            break;
        }
        case Instr::Kind::Try: {
            auto *b = in->as<Instr::Try>();
            if (k == 0) {
                e.byte(0x06);
                block_type(b->type);
                body = b->body;
                return true;
            }
            if (k - 1 < b->catches.size()) {
                const LegacyCatch &c = b->catches[k - 1];
                if (c.kind == LegacyCatch::Kind::LegacyCatch) {
                    e.byte(CATCH);
                    tag_index(c.tag.value);
                } else {
                    e.byte(CATCH_ALL);
                }
                body = c.body;
                return true;
            }
            break;
        }
        case Instr::Kind::TryDelegate: {
            auto *b = in->as<Instr::TryDelegate>();
            if (k == 0) {
                e.byte(0x06);
                block_type(b->type);
                body = b->body;
                return true;
            }
            e.byte(DELEGATE);
            e.uleb(b->label_out.value);
            return false;
        }
        default:
            plain(in);
            return false;
        }
        e.byte(END);
        return false;
    }

    // Index `x` of space `s`, the operand of `o`.
    void index(Opcode o, Space s, u32 x)
    {
        const OpDef &d = op_def(o);
        data_used      = data_used || d.x == Space::DATA || d.y == Space::DATA;
        switch (s) {
        case Space::FUNC:
            func_index(x);
            break;
        case Space::TABLE:
            table_index(x);
            break;
        case Space::GLOBAL:
            global_index(x);
            break;
        case Space::TAG:
            tag_index(x);
            break;
        case Space::TYPE:
            if (o == call_ref || o == return_call_ref)
                type_index(x);
            else
                e.uleb(x);
            break;
        default:
            e.uleb(x);
            break;
        }
    }

    void mem_arg(Opcode o, const Idx &memory, u64 offset, u64 align)
    {
        op(o);
        if (memory.value) {
            e.uleb(log2(align) | 0x40);
            e.uleb(memory.value);
        } else {
            e.uleb(log2(align));
        }
        e.uleb64(offset);
    }

    void plain(const Instr *in)
    {
        switch (in->kind) {
        case Instr::Kind::Plain: {
            Opcode o = in->as<Instr::Plain>()->op;
            op(o);
            if (o == fence)
                e.byte(0);
            break;
        }
        case Instr::Kind::Select: {
            auto *s = in->as<Instr::Select>();
            if (s->typed) {
                e.byte(0x1c);
                vals(s->results);
            } else {
                e.byte(0x1b);
            }
            break;
        }
        case Instr::Kind::Br: {
            auto *b = in->as<Instr::Br>();
            op(b->op);
            e.uleb(b->label.value);
            break;
        }
        case Instr::Kind::BrTable: {
            auto *b = in->as<Instr::BrTable>();
            e.byte(0x0e);
            e.uleb(b->labels.size());
            for (const Idx &x : b->labels)
                e.uleb(x.value);
            e.uleb(b->default_.value);
            break;
        }
        case Instr::Kind::BrOnCast: {
            auto *b = in->as<Instr::BrOnCast>();
            op(b->op);
            e.byte(u8((b->from.nullable ? 1 : 0) | (b->to.nullable ? 2 : 0)));
            e.uleb(b->label.value);
            heap(b->from.heap);
            heap(b->to.heap);
            break;
        }
        case Instr::Kind::Index: {
            auto *x = in->as<Instr::Index>();
            op(x->op);
            index(x->op, op_def(x->op).x, x->x.value);
            break;
        }
        case Instr::Kind::Index2: {
            // table.init and memory.init put the segment first.
            auto *x        = in->as<Instr::Index2>();
            const OpDef &d = op_def(x->op);
            op(x->op);
            if (d.imm == Imm::IDX_OPT_IDX) {
                index(x->op, d.y, x->y.value);
                index(x->op, d.x, x->x.value);
            } else {
                index(x->op, d.x, x->x.value);
                index(x->op, d.y, x->y.value);
            }
            break;
        }
        case Instr::Kind::ArrayNewFixed: {
            auto *a = in->as<Instr::ArrayNewFixed>();
            e.byte(0xfb);
            e.uleb(8);
            e.uleb(a->type.value);
            e.uleb(a->count);
            break;
        }
        case Instr::Kind::CallIndirect: {
            auto *c = in->as<Instr::CallIndirect>();
            op(c->op);
            type_index(c->type.type.value.value);
            table_index(c->table.value);
            break;
        }
        case Instr::Kind::MemArg: {
            auto *m = in->as<Instr::MemArg>();
            mem_arg(m->op, m->memory, m->offset, m->align);
            break;
        }
        case Instr::Kind::MemArgLane: {
            auto *m = in->as<Instr::MemArgLane>();
            mem_arg(m->op, m->memory, m->offset, m->align);
            e.byte(m->lane);
            break;
        }
        case Instr::Kind::I32Const:
            e.byte(0x41);
            e.sleb(i32(in->as<Instr::I32Const>()->value));
            break;
        case Instr::Kind::I64Const:
            e.byte(0x42);
            e.sleb64(i64(in->as<Instr::I64Const>()->value));
            break;
        case Instr::Kind::F32Const:
            e.byte(0x43);
            e.u32le(in->as<Instr::F32Const>()->bits);
            break;
        case Instr::Kind::F64Const: {
            u64 b = in->as<Instr::F64Const>()->bits;
            e.byte(0x44);
            e.u32le(u32(b));
            e.u32le(u32(b >> 32));
            break;
        }
        case Instr::Kind::V128Const: {
            Str s = in->as<Instr::V128Const>()->value;
            e.byte(0xfd);
            e.uleb(12);
            e.bytes(Bytes(reinterpret_cast<const u8 *>(s.data()), s.size()));
            break;
        }
        case Instr::Kind::Lane: {
            auto *l = in->as<Instr::Lane>();
            op(l->op);
            e.byte(l->lane);
            break;
        }
        case Instr::Kind::Shuffle: {
            e.byte(0xfd);
            e.uleb(13);
            for (u8 l : in->as<Instr::Shuffle>()->lanes)
                e.byte(l);
            break;
        }
        case Instr::Kind::RefNull:
            e.byte(0xd0);
            heap(in->as<Instr::RefNull>()->type);
            break;
        case Instr::Kind::RefTypeOp: {
            // The nullable form is the next opcode.
            auto *r = in->as<Instr::RefTypeOp>();
            op(r->op, r->type.nullable ? 1 : 0);
            heap(r->type.heap);
            break;
        }
        default:
            break;
        }
    }

    // ---------------------------------------------------- fields

    // Locals, one entry for each run of one type.
    void locals(const List<Local> &l)
    {
        u32 runs = 0;
        for (u32 i = 0; i < l.size(); i++)
            runs += i == 0 || !same(l[i].type, l[i - 1].type);
        e.uleb(runs);
        for (u32 i = 0; i < l.size();) {
            u32 j = i + 1;
            while (j < l.size() && same(l[j].type, l[i].type))
                j++;
            e.uleb(j - i);
            val(l[i].type);
            i = j;
        }
    }

    static bool same(const ValType &a, const ValType &b)
    {
        if (a.kind != b.kind)
            return false;
        if (a.kind != ValType::Kind::Ref)
            return true;
        const RefType &x = a.type, &y = b.type;
        if (x.nullable != y.nullable || x.heap.kind != y.heap.kind)
            return false;
        if (x.heap.kind == HeapType::Kind::Abstract)
            return x.heap.abs == y.heap.abs;
        return x.heap.index.value == y.heap.index.value;
    }

    // A list of `ref.func x`, as (ref func) and nothing else, is written as
    // function indices.
    bool as_funcs(const ElemList &l)
    {
        if (l.kind == ElemList::Kind::Funcs)
            return true;
        if (l.type.nullable || l.type.heap.kind != HeapType::Kind::Abstract ||
            l.type.heap.abs != AbsHeapType::HFunc)
            return false;
        for (const Expr &x : l.items) {
            if (x.instrs.size() != 1)
                return false;
            auto *r = x.instrs[0]->as<Instr::Index>();
            if (!r || r->op != ref_fn)
                return false;
        }
        return true;
    }

    void elem(const Decl::Elem &s)
    {
        const ElemList &l = s.elems;
        bool funcs        = as_funcs(l);
        u8 flags          = funcs ? 0 : 4;
        bool active       = s.mode.kind == ElemMode::Kind::ElemActive;
        if (s.mode.kind == ElemMode::Kind::ElemPassive)
            flags |= 1;
        else if (s.mode.kind == ElemMode::Kind::ElemDeclare)
            flags |= 3;
        else if (s.mode.table.value != 0 ||
                 (!funcs && (l.type.heap.kind != HeapType::Kind::Abstract ||
                             l.type.heap.abs != AbsHeapType::HFunc)))
            flags |= 2;
        e.byte(flags);
        if (active && (flags & 2))
            e.uleb(s.mode.table.value);
        if (active)
            expr(s.mode.offset);
        if (flags & 3) {
            if (funcs)
                e.byte(0x00);
            else
                ref(l.type);
        }
        if (l.kind == ElemList::Kind::Funcs) {
            e.uleb(l.funcs.size());
            for (const Idx &x : l.funcs)
                func_index(x.value);
        } else if (funcs) {
            e.uleb(l.items.size());
            for (const Expr &x : l.items)
                func_index(x.instrs[0]->as<Instr::Index>()->x.value);
        } else {
            e.uleb(l.items.size());
            for (const Expr &x : l.items)
                expr(x.instrs);
        }
    }

    void data(const Decl::Data &d)
    {
        if (d.mode.kind == DataMode::Kind::DataPassive) {
            e.byte(1);
        } else if (d.mode.memory.value) {
            e.byte(2);
            e.uleb(d.mode.memory.value);
            expr(d.mode.offset);
        } else {
            e.byte(0);
            expr(d.mode.offset);
        }
        e.name(d.init);
    }

    void import(const Decl::Import &i)
    {
        e.name(i.module);
        e.name(i.item);
        const Extern &x = i.desc;
        e.byte(u8(x.kind));
        switch (x.kind) {
        case Extern::Kind::FuncImport:
            e.uleb(x.type.type.value.value);
            break;
        case Extern::Kind::TableImport:
            table_type(x.table);
            break;
        case Extern::Kind::MemoryImport:
            mem_type(x.memory);
            break;
        case Extern::Kind::GlobalImport:
            global_type(x.global);
            break;
        case Extern::Kind::TagImport:
            e.byte(0);
            e.uleb(x.type.type.value.value);
            break;
        }
    }

    // ---------------------------------------------------- the module

    void module(const Module &m)
    {
        u32 n[14] = {};
        for (const Decl *d : m.decls)
            switch (d->kind) {
            case Decl::Kind::Type:
                n[TYPE_SEC]++;
                break;
            case Decl::Kind::Import:
                n[IMPORT_SEC]++;
                break;
            case Decl::Kind::Func:
                n[FUNC_SEC]++;
                break;
            case Decl::Kind::Table:
                n[TABLE_SEC]++;
                break;
            case Decl::Kind::Memory:
                n[MEMORY_SEC]++;
                break;
            case Decl::Kind::Global:
                n[GLOBAL_SEC]++;
                break;
            case Decl::Kind::Tag:
                n[TAG_SEC]++;
                break;
            case Decl::Kind::Export:
                n[EXPORT_SEC]++;
                break;
            case Decl::Kind::Start:
                n[START_SEC]++;
                break;
            case Decl::Kind::Elem:
                n[ELEM_SEC]++;
                break;
            case Decl::Kind::Data:
                n[DATA_SEC]++;
                break;
            case Decl::Kind::Custom:
                break;
            }

        e.u32le(0x6d736100); // \0asm
        e.u32le(1);
        if (object)
            symbolize(m);

        constexpr u8 ORDER[] = { TYPE_SEC,       IMPORT_SEC, FUNC_SEC,   TABLE_SEC, MEMORY_SEC,
                                 TAG_SEC,        GLOBAL_SEC, EXPORT_SEC, START_SEC, ELEM_SEC,
                                 DATA_COUNT_SEC, CODE_SEC,   DATA_SEC };
        usize count_at = 0, count_end = 0;
        for (u8 id : ORDER) {
            if (id == DATA_COUNT_SEC) {
                // Written only when code names a data segment, which the code
                // after it says.
                if (!n[DATA_SEC])
                    continue;
                count_at      = v.size();
                count_section = sections;
                usize at      = begin(id);
                e.uleb(n[DATA_SEC]);
                end(id, at);
                count_end = v.size();
                continue;
            }
            u32 count = id == CODE_SEC ? n[FUNC_SEC] : n[id];
            if (!count)
                continue;
            usize at = begin(id);
            if (id != START_SEC)
                e.uleb(count);
            section(m, id);
            end(id, at);
            if (id == CODE_SEC && n[DATA_SEC] && !data_used)
                drop_count(count_at, count_end);
        }
        if (n[DATA_SEC] && !n[FUNC_SEC] && !data_used)
            drop_count(count_at, count_end);
        if (object)
            linking();
    }

    // The data count section, which no code needed: the sections after it
    // move up a place.
    void drop_count(usize at, usize end)
    {
        v.erase(at, end - at);
        sections--;
        for (Reloc &r : relocs)
            if (r.section > count_section)
                r.section--;
    }

    // ---------------------------------------------------- the object

    // Decision 1 of Plan.md: wabt's rule, which gives every function, table
    // and global a symbol, and tags one too.
    void symbolize(const Module &m)
    {
        u32 count[4] = {};
        auto kind_of = [](const Decl *d) -> i32 {
            if (auto *i = d->as<Decl::Import>()) {
                switch (i->desc.kind) {
                case Extern::Kind::FuncImport:
                    return 0;
                case Extern::Kind::TableImport:
                    return 1;
                case Extern::Kind::GlobalImport:
                    return 2;
                case Extern::Kind::TagImport:
                    return 3;
                default:
                    return -1;
                }
            }
            switch (d->kind) {
            case Decl::Kind::Func:
                return 0;
            case Decl::Kind::Table:
                return 1;
            case Decl::Kind::Global:
                return 2;
            case Decl::Kind::Tag:
                return 3;
            default:
                return -1;
            }
        };
        for (const Decl *d : m.decls)
            if (i32 k = kind_of(d); k >= 0)
                count[k]++;
        for (u32 k = 1; k < 4; k++)
            first[k] = first[k - 1] + count[k - 1];
        if (!symbols.resize(first[3] + count[3]))
            e.oom = true;
        Vec<bool> exported;
        if (!exported.resize(symbols.size()))
            e.oom = true;
        if (e.oom)
            return;
        constexpr i32 SORT[] = { 0, 1, -1, 2, 3 };
        for (const Decl *d : m.decls)
            if (auto *x = d->as<Decl::Export>()) {
                i32 k = SORT[u8(x->sort)];
                if (k >= 0 && x->index.value < count[k])
                    exported[first[k] + x->index.value] = true;
            }
        constexpr u8 KIND[] = { wasm::SYM_FUNCTION, wasm::SYM_TABLE, wasm::SYM_GLOBAL,
                                wasm::SYM_TAG };
        u32 at[4]           = {};
        for (const Decl *d : m.decls) {
            i32 k = kind_of(d);
            if (k < 0)
                continue;
            auto *i       = d->as<Decl::Import>();
            const Bind &b = i ? i->desc.bind : bind(d);
            u32 n         = first[k] + at[k];
            Symbol &s     = symbols[n];
            s.kind        = KIND[k];
            s.index       = at[k]++;
            if (i) {
                s.flags = wasm::SYM_UNDEFINED;
            } else {
                if (!b.id.has)
                    s.flags |= wasm::SYM_LOCAL | wasm::SYM_HIDDEN;
                else
                    s.name = b.id.value;
                if (exported[n])
                    s.flags |= wasm::SYM_HIDDEN | wasm::SYM_NO_STRIP;
            }
            if (exported[n])
                s.flags |= wasm::SYM_EXPORTED;
        }
    }

    static const Bind &bind(const Decl *d)
    {
        switch (d->kind) {
        case Decl::Kind::Func:
            return d->as<Decl::Func>()->bind;
        case Decl::Kind::Table:
            return d->as<Decl::Table>()->bind;
        case Decl::Kind::Global:
            return d->as<Decl::Global>()->bind;
        default:
            return d->as<Decl::Tag>()->bind;
        }
    }

    // The `linking` section, and a `reloc.` section for each section with
    // relocations.
    void linking()
    {
        usize at = begin(0);
        e.name("linking");
        e.uleb(2);
        if (!symbols.empty()) {
            e.byte(wasm::SUB_SYMBOL_TABLE);
            usize sub = v.size();
            e.uleb(symbols.size());
            for (const Symbol &s : symbols) {
                e.byte(s.kind);
                e.uleb(s.flags);
                e.uleb(s.index);
                if (!(s.flags & wasm::SYM_UNDEFINED))
                    e.name(s.name);
            }
            size(sub);
        }
        end(0, at);
        for (usize i = 0; i < relocs.size();) {
            usize j = i;
            while (j < relocs.size() && relocs[j].section == relocs[i].section)
                j++;
            Str name = wasm::section_name(relocs[i].id);
            at       = begin(0);
            e.uleb(6 + name.size());
            e.bytes(Bytes(reinterpret_cast<const u8 *>("reloc."), 6));
            e.bytes(Bytes(reinterpret_cast<const u8 *>(name.data()), name.size()));
            e.uleb(relocs[i].section);
            e.uleb(j - i);
            for (usize k = i; k < j; k++) {
                e.uleb(relocs[k].type);
                e.uleb(u32(relocs[k].at));
                e.uleb(relocs[k].index);
            }
            end(0, at);
            i = j;
        }
    }

    void section(const Module &m, u8 id)
    {
        for (const Decl *d : m.decls) {
            if (e.oom)
                return;
            switch (d->kind) {
            case Decl::Kind::Type:
                if (id == TYPE_SEC)
                    group(d->as<Decl::Type>()->group);
                break;
            case Decl::Kind::Import:
                if (id == IMPORT_SEC)
                    import(*d->as<Decl::Import>());
                break;
            case Decl::Kind::Func: {
                auto *f = d->as<Decl::Func>();
                if (id == FUNC_SEC) {
                    e.uleb(f->type.type.value.value);
                } else if (id == CODE_SEC) {
                    usize at = v.size();
                    locals(f->locals);
                    seq(f->body);
                    e.byte(END);
                    size(at);
                }
                break;
            }
            case Decl::Kind::Table: {
                auto *t = d->as<Decl::Table>();
                if (id != TABLE_SEC)
                    break;
                if (t->init.empty()) {
                    table_type(t->type);
                } else {
                    e.byte(0x40);
                    e.byte(0x00);
                    table_type(t->type);
                    expr(t->init);
                }
                break;
            }
            case Decl::Kind::Memory:
                if (id == MEMORY_SEC)
                    mem_type(d->as<Decl::Memory>()->type);
                break;
            case Decl::Kind::Global:
                if (id == GLOBAL_SEC) {
                    auto *g = d->as<Decl::Global>();
                    global_type(g->type);
                    expr(g->init);
                }
                break;
            case Decl::Kind::Tag:
                if (id == TAG_SEC) {
                    e.byte(0);
                    e.uleb(d->as<Decl::Tag>()->type.type.value.value);
                }
                break;
            case Decl::Kind::Export:
                if (id == EXPORT_SEC) {
                    auto *x = d->as<Decl::Export>();
                    e.name(x->name);
                    e.byte(u8(x->sort));
                    e.uleb(x->index.value);
                }
                break;
            case Decl::Kind::Start:
                if (id == START_SEC)
                    e.uleb(d->as<Decl::Start>()->func.value);
                break;
            case Decl::Kind::Elem:
                if (id == ELEM_SEC)
                    elem(*d->as<Decl::Elem>());
                break;
            case Decl::Kind::Data:
                if (id == DATA_SEC)
                    data(*d->as<Decl::Data>());
                break;
            case Decl::Kind::Custom:
                break;
            }
        }
    }
};

} // namespace

bool encode(const Module &m, bool object, Vec<u8> &out, Diag &diag)
{
    Encoder c(out, object);
    c.module(m);
    if (c.e.oom) {
        diag.error("out of memory");
        return false;
    }
    return true;
}
