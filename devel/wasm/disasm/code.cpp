#include "code.h"

#include "cursor.h"
#include "demangle/demangle.h"
#include "math/ftoa.h"
#include "opcodes.h"

namespace {

// Reads as llvm's disassembler reads: a field that fails leaves `at` where
// it was, so what was taken before it is the instruction's size.
struct Reader {
    Bytes b;
    usize at = 0;

    bool byte(u8 &v)
    {
        if (at >= b.size())
            return false;
        v = b[at++];
        return true;
    }

    // decodeULEB128: any number of bytes, but no bits past the 64th.
    bool uleb(u64 &v)
    {
        u64 value = 0;
        u32 shift = 0;
        usize p   = at;
        u8 byte;
        do {
            if (p >= b.size())
                return false;
            byte      = b[p++];
            u64 slice = byte & 0x7f;
            if ((shift == 63 && slice > 1) || (shift > 63 && slice != 0))
                return false;
            if (shift < 64)
                value += slice << shift;
            shift += 7;
        } while (byte >= 128);
        at = p;
        v  = value;
        return true;
    }

    // decodeSLEB128.
    bool sleb(i64 &v)
    {
        u64 value = 0;
        u32 shift = 0;
        usize p   = at;
        u8 byte;
        do {
            if (p >= b.size())
                return false;
            byte      = b[p++];
            u64 slice = byte & 0x7f;
            bool neg  = i64(value) < 0;
            if ((shift == 63 && slice != 0 && slice != 0x7f) ||
                (shift > 63 && slice != (neg ? 0x7f : 0)))
                return false;
            if (shift < 64)
                value |= slice << shift;
            shift += 7;
        } while (byte >= 128);
        if (shift < 64 && (byte & 0x40))
            value |= ~u64(0) << shift;
        at = p;
        v  = i64(value);
        return true;
    }

    // A little-endian field of `n` bytes.
    bool fixed(usize n, u64 &v)
    {
        if (b.size() - at < n)
            return false;
        v = 0;
        for (usize i = 0; i < n; i++)
            v |= u64(b[at + i]) << (8 * i);
        at += n;
        return true;
    }
};

Str type_name(u64 t)
{
    switch (t) {
    case 0x7f:
        return "i32";
    case 0x7e:
        return "i64";
    case 0x7d:
        return "f32";
    case 0x7c:
        return "f64";
    case 0x7b:
        return "v128";
    case 0x70:
        return "funcref";
    case 0x6f:
        return "externref";
    case 0x69:
        return "exnref";
    default:
        return "invalid_type";
    }
}

// A heap type: an abbreviation, or a type index.
void put_heap(Out &o, i64 h)
{
    if (h >= 0) {
        put_i64(o, h);
        return;
    }
    switch (h & 0x7f) {
    case 0x70:
        o.put("func");
        break;
    case 0x6f:
        o.put("extern");
        break;
    case 0x69:
        o.put("exn");
        break;
    case 0x6e:
        o.put("any");
        break;
    case 0x6d:
        o.put("eq");
        break;
    case 0x6c:
        o.put("i31");
        break;
    case 0x6b:
        o.put("struct");
        break;
    case 0x6a:
        o.put("array");
        break;
    case 0x71:
        o.put("none");
        break;
    case 0x72:
        o.put("noextern");
        break;
    case 0x73:
        o.put("nofunc");
        break;
    case 0x74:
        o.put("noexn");
        break;
    default:
        o.put("invalid_type");
        break;
    }
}

// A function type's signature: "(i32, i32) -> i32".
void put_signature(Out &o, const FuncType &t)
{
    auto list = [&](Bytes b, bool parens) {
        Cursor c(b);
        u32 n = 0;
        Out s;
        while (!c.done() && c.ok()) {
            if (n++)
                s.put(", ");
            put_valtype(s, c);
        }
        if (parens || n != 1)
            o.put('(').put(s.str()).put(')');
        else
            o.put(s.str());
    };
    list(t.params, true);
    o.put(" -> ");
    list(t.results, false);
}

// A block type: a value type in one byte, none (0x40, printed as nothing),
// or a type index, printed as its signature.
bool block_type(Reader &r, Out &o, const Ctx &ctx)
{
    usize at = r.at;
    i64 v;
    if (!r.sleb(v))
        return false;
    if (v >= 0) {
        if (u64(v) < ctx.types.size()) {
            put_signature(o, ctx.types[v]);
        } else {
            o.put("type=");
            put_i64(o, v);
        }
    } else if (r.at != at + 1) {
        o.put("invalid_type");
    } else if ((v & 0x7f) != 0x40) {
        o.put(type_name(u64(v & 0x7f)));
    }
    return true;
}

// A float as APFloat's convertToHexString writes a double: the significand's
// hex digits up to its last one set, and a decimal binary exponent.
void put_double(Out &o, u64 bits)
{
    u32 exp = (bits >> 52) & 0x7ff;
    u64 man = bits & ((u64(1) << 52) - 1);
    if (bits >> 63)
        o.put('-');
    if (exp == 0 && man == 0) {
        o.put("0x0p0");
        return;
    }
    u64 sig = exp ? man | u64(1) << 52 : man;
    i64 e   = exp ? i64(exp) - 1023 : -1022;
    u32 lsb = 0;
    while (!((sig >> lsb) & 1))
        lsb++;
    u32 digits = (59 - lsb) / 4;
    o.put("0x").put("0123456789abcdef"[sig >> 52]);
    if (digits > 1)
        o.put('.');
    for (u32 k = 1; k < digits; k++)
        o.put("0123456789abcdef"[(sig >> (52 - 4 * k)) & 0xf]);
    o.put('p');
    put_i64(o, e);
}

// A float widened to a double: exactly, a subnormal normalised.
u64 widen(u32 f)
{
    u64 sign = u64(f >> 31) << 63;
    u32 exp  = (f >> 23) & 0xff;
    u64 man  = f & 0x7fffff;
    if (exp == 0) {
        if (!man)
            return sign;
        i64 e = -126;
        while (!(man & 0x800000)) {
            man <<= 1;
            e--;
        }
        return sign | u64(e + 1023) << 52 | (man & 0x7fffff) << 29;
    }
    return sign | u64(exp - 127 + 1023) << 52 | man << 29;
}

// A float constant: the shortest decimal that reads back to it, and its
// exact value in hex among the notes. `man_bits` is 23 or 52.
void put_float(Out &o, Out &notes, u64 bits, u32 man_bits)
{
    u32 exp_bits = man_bits == 23 ? 8 : 11;
    u64 man      = bits & ((u64(1) << man_bits) - 1);
    u64 exp      = (bits >> man_bits) & ((u64(1) << exp_bits) - 1);
    bool neg     = bits >> (man_bits + exp_bits);
    if (neg)
        o.put('-');
    if (exp == (u64(1) << exp_bits) - 1) {
        if (!man) {
            o.put("inf");
        } else if (man == u64(1) << (man_bits - 1)) {
            o.put("nan");
        } else {
            o.put("nan:0x");
            put_hex(o, man, 0);
        }
        return;
    }
    char t[64];
    Str s;
    if (man_bits == 52) {
        s = fmt_f64_shortest(t, sizeof t, __builtin_bit_cast(f64, bits & ~(u64(1) << 63)));
    } else {
        f32 v = __builtin_bit_cast(f32, u32(bits & 0x7fffffff));
        for (i32 prec = 1; prec <= 9; prec++) {
            s = fmt_f64(t, sizeof t, f64(v), prec, 'g');
            usize used;
            Option<f32> back = scan_f32(s, used);
            if (back.has_value() && __builtin_bit_cast(u32, back.value()) == u32(bits & 0x7fffffff))
                break;
        }
    }
    o.put(s);
    if (exp || man) {
        put_double(notes, man_bits == 52 ? bits : widen(u32(bits)));
        notes.put('\n');
    }
}

struct Decoder {
    Reader r;
    Ctx &ctx;
    Flow &flow;
    Out &ops;
    Out &notes;
    Vec<u64> printed; // depths already annotated

    void note(Str s) { notes.put(s).put('\n'); }

    void name(const Vec<Str> &names, u64 i)
    {
        if (i >= names.size() || names[i].empty())
            return;
        Str n = names[i];
        if (ctx.demangle) {
            demangle(n, ctx.scratch);
            n = ctx.scratch.str();
        }
        note(n);
    }

    void signature(u64 t)
    {
        if (t >= ctx.types.size())
            return;
        put_signature(notes, ctx.types[t]);
        notes.put('\n');
    }

    const Flow::Frame *frame(u64 depth) const
    {
        if (depth >= flow.stack.size())
            return nullptr;
        return &flow.stack[flow.stack.size() - 1 - depth];
    }

    // "<depth>: down to label<n>", once for each depth.
    void branch(u64 depth)
    {
        for (u64 d : printed)
            if (d == depth)
                return;
        printed.push(depth);
        put_u64(notes, depth);
        if (const Flow::Frame *f = frame(depth)) {
            notes.put(f->kind == Flow::LOOP ? ": up to label"_s : ": down to label"_s);
            put_u64(notes, f->label);
        } else if (depth == flow.stack.size()) {
            notes.put(": return");
        } else {
            notes.put(": invalid depth");
        }
        notes.put('\n');
    }

    void label(Str what, u32 n)
    {
        notes.put(what);
        put_u64(notes, n);
        notes.put(":\n");
    }

    bool push(Flow::Kind kind)
    {
        if (kind == Flow::LOOP)
            label("label", flow.counter);
        return flow.stack.push({ flow.counter++, kind, Flow::NONE });
    }

    // Where a branch to the block lands, but for a loop's, which is its start.
    void end()
    {
        if (flow.stack.empty())
            return;
        Flow::Frame f = flow.stack.back();
        flow.stack.pop();
        if (f.kind != Flow::LOOP)
            label("label", f.label);
    }

    void catches(bool all)
    {
        if (flow.stack.empty() || flow.stack.back().kind != Flow::TRY) {
            note("catch without try");
            return;
        }
        Flow::Frame &f = flow.stack.back();
        if (f.eh == Flow::CATCH_ALL) {
            note("catch after catch_all");
            return;
        }
        f.eh = all ? Flow::CATCH_ALL : Flow::CATCH;
        label("catch", f.label);
    }

    void rethrow(u64 depth)
    {
        const Flow::Frame *f = frame(depth);
        put_u64(notes, depth);
        if (f && f->kind == Flow::TRY && f->eh != Flow::NONE) {
            notes.put(": from catch");
            put_u64(notes, f->label);
        } else {
            notes.put(f ? ": not a catch"_s : ": invalid depth"_s);
        }
        notes.put('\n');
    }

    void delegate(u64 depth)
    {
        if (flow.stack.empty() || flow.stack.back().kind != Flow::TRY) {
            note("delegate without try");
            return;
        }
        end();
        put_u64(notes, depth);
        if (const Flow::Frame *f = frame(depth)) {
            notes.put(f->kind == Flow::TRY ? ": to catch"_s : ": out of label"_s);
            put_u64(notes, f->label);
        } else if (depth == flow.stack.size()) {
            notes.put(": to caller");
        } else {
            notes.put(": invalid depth");
        }
        notes.put('\n');
    }

    void sep()
    {
        if (ops.s.size())
            ops.put(' ');
    }

    // The ordering, the offset where not 0, the alignment where not the
    // natural one, and the lane.
    bool memarg(const OpInfo &op, bool atomic, bool lane)
    {
        u64 align, off;
        u8 order = 0;
        if (!r.uleb(align))
            return false;
        if (atomic && (align & 0x20)) {
            if (!r.byte(order))
                return false;
            align &= ~u64(0x20);
        }
        if (!r.uleb(off))
            return false;
        u8 l = 0;
        if (lane && !r.byte(l))
            return false;
        if (order == 1)
            ops.put("acqrel");
        if (off) {
            sep();
            ops.put("offset=");
            put_u64(ops, off);
        }
        if (align != op.align) {
            sep();
            if (align < 64) {
                ops.put("align=");
                put_u64(ops, u64(1) << align);
            } else {
                ops.put("align=2**");
                put_u64(ops, align);
            }
        }
        if (lane) {
            sep();
            put_u64(ops, l);
        }
        return true;
    }

    bool br_table()
    {
        u64 n, t;
        if (!r.uleb(n))
            return false;
        // The targets, then the default.
        ops.put('{');
        for (u64 i = 0; i <= n; i++) {
            if (!r.uleb(t))
                return false;
            if (i)
                ops.put(", ");
            put_u64(ops, t);
            branch(t);
        }
        ops.put('}');
        return true;
    }

    bool try_table()
    {
        u64 n;
        if (!block_type(r, ops, ctx) || !r.uleb(n))
            return false;
        for (u64 i = 0; i < n; i++) {
            u8 kind;
            u64 tag = 0, depth;
            if (!r.byte(kind))
                return false;
            if (kind <= 1 && !r.uleb(tag))
                return false;
            if (!r.uleb(depth))
                return false;
            static const Str KINDS[] = { "catch", "catch_ref", "catch_all", "catch_all_ref" };
            sep();
            ops.put('(');
            if (kind <= 3)
                ops.put(KINDS[kind]).put(' ');
            if (kind <= 1) {
                put_u64(ops, tag);
                ops.put(' ');
            }
            put_u64(ops, depth);
            ops.put(')');
            branch(depth);
        }
        return push(Flow::TRY_TABLE);
    }

    bool index(const Vec<Str> *names)
    {
        u64 a;
        if (!r.uleb(a))
            return false;
        put_u64(ops, a);
        if (names)
            name(*names, a);
        return true;
    }

    bool operands(const OpInfo &op)
    {
        u64 a, b;
        i64 s;
        u8 v;
        switch (op.kind) {
        case OpKind::NONE:
            return true;
        case OpKind::END:
            end();
            return true;
        case OpKind::ULEB:
            return index(nullptr);
        case OpKind::FUNC:
            return index(&ctx.funcs);
        case OpKind::GLOBAL:
            return index(&ctx.globals);
        case OpKind::TABLE:
            return index(&ctx.tables);
        case OpKind::TAG:
            return index(&ctx.tags);
        case OpKind::TYPE:
            if (!r.uleb(a))
                return false;
            put_u64(ops, a);
            signature(a);
            return true;
        case OpKind::ULEB2:
        case OpKind::TABLE2:
            if (!r.uleb(a) || !r.uleb(b))
                return false;
            put_u64(ops, a);
            ops.put(", ");
            put_u64(ops, b);
            if (op.kind == OpKind::TABLE2) {
                name(ctx.tables, a);
                name(ctx.tables, b);
            }
            return true;
        case OpKind::SLEB2:
            if (!r.sleb(s))
                return false;
            put_i64(ops, s);
            ops.put(", ");
            [[fallthrough]];
        case OpKind::SLEB:
        case OpKind::I32:
        case OpKind::I64:
            if (!r.sleb(s))
                return false;
            put_i64(ops, s);
            return true;
        case OpKind::F32:
            if (!r.fixed(4, a))
                return false;
            put_float(ops, notes, a, 23);
            return true;
        case OpKind::F64:
            if (!r.fixed(8, a))
                return false;
            put_float(ops, notes, a, 52);
            return true;
        case OpKind::MEM:
            return memarg(op, false, false);
        case OpKind::MEM_LANE:
            return memarg(op, false, true);
        case OpKind::ATOMIC:
            return memarg(op, true, false);
        case OpKind::LANE:
            if (!r.byte(v))
                return false;
            put_u64(ops, v);
            return true;
        case OpKind::V128:
            for (u32 i = 0; i < 4; i++) {
                if (!r.fixed(4, a))
                    return false;
                if (i)
                    ops.put(", ");
                put_u64(ops, a);
            }
            return true;
        case OpKind::SHUFFLE:
            for (u32 i = 0; i < 16; i++) {
                if (!r.byte(v))
                    return false;
                if (i)
                    ops.put(", ");
                put_u64(ops, v);
            }
            return true;
        case OpKind::FENCE:
            if (!r.byte(v))
                return false;
            if (v == 1)
                ops.put("acqrel");
            return true;
        case OpKind::IF:
        case OpKind::BLOCK:
        case OpKind::LOOP:
        case OpKind::TRY:
            if (!block_type(r, ops, ctx))
                return false;
            return push(op.kind == OpKind::IF     ? Flow::IF
                        : op.kind == OpKind::LOOP ? Flow::LOOP
                        : op.kind == OpKind::TRY  ? Flow::TRY
                                                  : Flow::BLOCK);
        case OpKind::BR:
            if (!r.uleb(a))
                return false;
            put_u64(ops, a);
            branch(a);
            return true;
        case OpKind::BR_TABLE:
            return br_table();
        case OpKind::CALL_INDIRECT:
            if (!r.uleb(a) || !r.uleb(b))
                return false;
            ops.put("type=");
            put_u64(ops, a);
            ops.put(" table=");
            put_u64(ops, b);
            signature(a);
            return true;
        case OpKind::CATCH:
            if (!r.uleb(a))
                return false;
            put_u64(ops, a);
            catches(false);
            name(ctx.tags, a);
            return true;
        case OpKind::CATCH_ALL:
            catches(true);
            return true;
        case OpKind::RETHROW:
            if (!r.uleb(a))
                return false;
            put_u64(ops, a);
            rethrow(a);
            return true;
        case OpKind::DELEGATE:
            if (!r.uleb(a))
                return false;
            put_u64(ops, a);
            delegate(a);
            return true;
        case OpKind::SELECT_T:
            if (!r.uleb(a))
                return false;
            for (u64 i = 0; i < a; i++) {
                if (!r.byte(v))
                    return false;
                if (i)
                    ops.put(' ');
                ops.put(type_name(v));
            }
            return true;
        case OpKind::REF_NULL:
            if (!r.uleb(a))
                return false;
            if (a != 0x70 && a != 0x6f && a != 0x69)
                return false;
            put_heap(ops, i64(a) - 0x80);
            return true;
        case OpKind::TRY_TABLE:
            return try_table();
        }
        return false;
    }
};
} // namespace

void put_u64(Out &o, u64 v)
{
    char d[20];
    usize k = 0;
    do {
        d[k++] = char('0' + v % 10);
        v /= 10;
    } while (v);
    while (k)
        o.put(d[--k]);
}

void put_i64(Out &o, i64 v)
{
    if (v < 0) {
        o.put('-');
        put_u64(o, ~u64(v) + 1);
    } else {
        put_u64(o, u64(v));
    }
}

void put_hex(Out &o, u64 v, usize digits)
{
    char d[16];
    usize k = 0;
    do {
        d[k++] = "0123456789abcdef"[v & 0xf];
        v >>= 4;
    } while (v);
    for (usize i = k; i < digits; i++)
        o.put('0');
    while (k)
        o.put(d[--k]);
}

void put_rhex(Out &o, u64 v, usize width)
{
    usize k = 0;
    for (u64 x = v; k == 0 || x; x >>= 4)
        k++;
    for (usize i = k; i < width; i++)
        o.put(' ');
    put_hex(o, v, 0);
}

usize column(Str s)
{
    usize c = 0;
    // As formatted_raw_ostream counts: a tab has no width of its own, so one
    // at a tab stop does not move.
    for (char ch : s)
        c = ch == '\t' ? (c + 7) / 8 * 8 : c + 1;
    return c;
}

void pad_to(Out &o, usize at, usize to)
{
    for (usize i = at; i < to || i == at; i++)
        o.put(' ');
}

bool put_valtype(Out &o, Cursor &c)
{
    u8 t = c.byte();
    if (t == 0x63 || t == 0x64) {
        i64 h = c.sleb64();
        o.put(t == 0x63 ? "(ref null "_s : "(ref "_s);
        put_heap(o, h);
        o.put(')');
    } else {
        o.put(type_name(t));
    }
    return c.ok() && type_name(t) != "invalid_type"_s;
}

usize decode(Bytes b, Ctx &ctx, Out &text, Out &notes, bool &ok)
{
    Out &ops = ctx.ops;
    ops.s.clear();
    Decoder d{ Reader{ b }, ctx, ctx.flow, ops, notes, Vec<u64>() };
    ok = false;
    u8 op;
    if (!d.r.byte(op))
        return 0;
    u8 prefix = 0;
    u64 sub   = op;
    if (op >= 0xfb && op <= 0xfe) {
        prefix = op;
        // The prefixed tables hold 256 entries.
        if (!d.r.uleb(sub) || sub >= 256) {
            text.put("\t<unknown>");
            return d.r.at;
        }
    }
    const OpInfo *info = find_op(prefix, u32(sub));
    if (info)
        ok = d.operands(*info);
    if (!ok) {
        notes.clear();
        text.put("\t<unknown>");
        return d.r.at;
    }
    text.put('\t').put(info->name);
    if (ops.s.size())
        text.put('\t').put(ops.str());
    return d.r.at;
}
