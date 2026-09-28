#include "code.h"

#include "opcodes.h"

namespace {

enum Eh : u8 { EH_TRY, EH_CATCH, EH_CATCH_ALL };

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

// WebAssembly::anyTypeToString.
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
    case 0x60:
        return "func";
    case 0x40:
        return "void";
    default:
        return "invalid_type";
    }
}

// A block type: a value type in one byte, none (0x40, printed as nothing),
// or a type index, which the disassembler cannot resolve.
bool block_type(Reader &r, Out &o)
{
    usize at = r.at;
    i64 v;
    if (!r.sleb(v))
        return false;
    if (v >= 0)
        o.put("unknown_type");
    else if (r.at != at + 1)
        o.put("invalid_type");
    else if ((v & 0x7f) != 0x40)
        o.put(type_name(u64(v & 0x7f)));
    return true;
}

// A float as APFloat's convertToHexString writes a double: the significand's
// hex digits up to its last one set, and a decimal binary exponent.
void put_double(Out &o, u64 bits)
{
    bool neg = bits >> 63;
    u32 exp  = (bits >> 52) & 0x7ff;
    u64 man  = bits & ((u64(1) << 52) - 1);
    // A NaN with a payload of its own.
    if (exp == 0x7ff && man && man != u64(1) << 51) {
        if (neg)
            o.put('-');
        o.put("nan:0x");
        char d[16];
        usize k = 0;
        do {
            d[k++] = "0123456789abcdef"[man & 0xf];
            man >>= 4;
        } while (man);
        while (k)
            o.put(d[--k]);
        return;
    }
    if (neg)
        o.put('-');
    if (exp == 0x7ff) {
        o.put(man ? "nan"_s : "infinity"_s);
        return;
    }
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

// A float widened to a double, as the disassembler widens it: exactly, a
// subnormal normalised, a NaN quieted.
u64 widen(u32 f)
{
    u64 sign = u64(f >> 31) << 63;
    u32 exp  = (f >> 23) & 0xff;
    u64 man  = f & 0x7fffff;
    if (exp == 0xff)
        return sign | u64(0x7ff) << 52 | (man ? man << 29 | u64(1) << 51 : 0);
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

struct Decoder {
    Reader r;
    Flow &flow;
    Out &text;
    Out &notes;
    Vec<u64> printed; // depths already annotated

    void note(Str s) { notes.put(s).put('\n'); }

    // "<depth>: down to label<n>", once for each depth.
    void branch(u64 depth)
    {
        for (u64 d : printed)
            if (d == depth)
                return;
        printed.push(depth);
        if (depth >= flow.stack.size()) {
            note("Invalid depth argument!");
            return;
        }
        const Flow::Frame &f = flow.stack[flow.stack.size() - 1 - depth];
        put_u64(notes, depth);
        notes.put(": ").put(f.loop ? "up"_s : "down"_s).put(" to label");
        put_u64(notes, f.label);
        notes.put('\n');
    }

    void push(bool loop)
    {
        if (loop) {
            notes.put("label");
            put_u64(notes, flow.counter);
            notes.put(":\n");
        }
        flow.stack.push({ flow.counter++, loop });
    }

    void catches(bool all)
    {
        if (flow.eh.empty()) {
            note("try-catch mismatch!");
        } else if (flow.eh.back() == EH_CATCH_ALL) {
            note("catch/catch_all cannot occur after catch_all");
        } else if (flow.eh.back() == EH_TRY) {
            if (flow.tries.empty()) {
                note("try-catch mismatch!");
            } else {
                notes.put("catch");
                put_u64(notes, flow.tries.back());
                notes.put(":\n");
                flow.tries.pop();
            }
            flow.eh.pop();
            flow.eh.push(all ? EH_CATCH_ALL : EH_CATCH);
        }
    }

    void delegate(u64 depth)
    {
        if (flow.stack.empty() || flow.tries.empty() || flow.eh.empty()) {
            note("try-delegate mismatch!");
            return;
        }
        Out label;
        label.put("label/catch");
        put_u64(label, flow.stack.back().label);
        label.put(": ");
        flow.stack.pop();
        flow.tries.pop();
        flow.eh.pop();
        if (depth >= flow.stack.size()) {
            label.put("to caller");
        } else {
            const Flow::Frame &f = flow.stack[flow.stack.size() - 1 - depth];
            if (f.loop) {
                note("delegate cannot target a loop");
            } else {
                label.put("down to catch");
                put_u64(label, f.label);
            }
        }
        note(label.str());
    }

    // Alignment, printed only where it is not the natural one, and offset.
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
        if (atomic)
            text.put(order == 1 ? "acqrel"_s : ""_s).put(' ');
        put_i64(text, i64(off));
        if (align != op.align) {
            text.put(":p2align=");
            put_i64(text, i64(align));
        }
        if (lane) {
            text.put(", ");
            put_u64(text, l);
        }
        return true;
    }

    bool br_table()
    {
        u64 n, t;
        if (!r.uleb(n))
            return false;
        // The targets, then the default. The first is not annotated: it is
        // the fixed operand, whose type is the list's.
        Vec<u64> targets;
        for (u64 i = 0; i <= n; i++) {
            if (!r.uleb(t))
                return false;
            targets.push(t);
        }
        text.put('{');
        for (usize i = 0; i < targets.size(); i++) {
            if (i)
                text.put(", ");
            put_i64(text, i64(targets[i]));
        }
        text.put('}');
        for (usize i = 1; i < targets.size(); i++)
            branch(targets[i]);
        return true;
    }

    bool try_table()
    {
        Out sig;
        u64 n;
        if (!block_type(r, sig) || !r.uleb(n))
            return false;
        text.put(sig.str()).put(' ');
        Vec<u64> labels;
        for (u64 i = 0; i < n; i++) {
            u8 kind;
            u64 tag = 0, label;
            if (!r.byte(kind))
                return false;
            if (kind <= 1 && !r.uleb(tag))
                return false;
            if (!r.uleb(label))
                return false;
            static const Str KINDS[] = { "catch", "catch_ref", "catch_all", "catch_all_ref" };
            if (i)
                text.put(' ');
            text.put('(');
            if (kind <= 3)
                text.put(KINDS[kind]).put(' ');
            if (kind <= 1) {
                put_i64(text, i64(tag));
                text.put(' ');
            }
            put_i64(text, i64(label));
            text.put(')');
            labels.push(label);
        }
        for (u64 l : labels)
            branch(l);
        push(false);
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
        case OpKind::ULEB:
            if (!r.uleb(a))
                return false;
            put_i64(text, i64(a));
            return true;
        case OpKind::ULEB2:
            if (!r.uleb(a) || !r.uleb(b))
                return false;
            put_i64(text, i64(a));
            text.put(", ");
            put_i64(text, i64(b));
            return true;
        case OpKind::SLEB2:
            if (!r.sleb(s))
                return false;
            put_i64(text, s);
            text.put(", ");
            [[fallthrough]];
        case OpKind::SLEB:
        case OpKind::I32:
        case OpKind::I64:
            if (!r.sleb(s))
                return false;
            put_i64(text, s);
            return true;
        case OpKind::F32:
            if (!r.fixed(4, a))
                return false;
            put_double(text, widen(u32(a)));
            return true;
        case OpKind::F64:
            if (!r.fixed(8, a))
                return false;
            put_double(text, a);
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
            put_u64(text, v);
            return true;
        case OpKind::V128:
            for (u32 i = 0; i < 4; i++) {
                if (!r.fixed(4, a))
                    return false;
                if (i)
                    text.put(", ");
                put_u64(text, a);
            }
            return true;
        case OpKind::SHUFFLE:
            for (u32 i = 0; i < 16; i++) {
                if (!r.byte(v))
                    return false;
                if (i)
                    text.put(", ");
                put_u64(text, v);
            }
            return true;
        case OpKind::FENCE:
            if (!r.byte(v))
                return false;
            text.put(v == 1 ? "acqrel"_s : ""_s);
            return true;
        case OpKind::SIG:
            return block_type(r, text);
        case OpKind::BLOCK:
        case OpKind::LOOP:
            if (!block_type(r, text))
                return false;
            push(op.kind == OpKind::LOOP);
            return true;
        case OpKind::TRY:
            if (!block_type(r, text))
                return false;
            flow.tries.push(flow.counter);
            flow.eh.push(EH_TRY);
            push(false);
            return true;
        case OpKind::BR:
            if (!r.uleb(a))
                return false;
            put_i64(text, i64(a));
            branch(a);
            return true;
        case OpKind::BR_TABLE:
            return br_table();
        case OpKind::CALL_INDIRECT:
            // The table goes unprinted.
            if (!r.uleb(a) || !r.uleb(b))
                return false;
            put_i64(text, i64(a));
            return true;
        case OpKind::CATCH:
            if (!r.uleb(a))
                return false;
            put_i64(text, i64(a));
            catches(false);
            return true;
        case OpKind::CATCH_ALL:
            catches(true);
            return true;
        case OpKind::RETHROW:
            if (!r.uleb(a))
                return false;
            put_i64(text, i64(a));
            if (flow.tries.empty()) {
                note("to caller");
            } else {
                notes.put("down to catch");
                put_u64(notes, flow.tries.back());
                notes.put('\n');
            }
            return true;
        case OpKind::DELEGATE:
            if (!r.uleb(a))
                return false;
            put_i64(text, i64(a));
            delegate(a);
            return true;
        case OpKind::SELECT_T:
            if (!r.uleb(a))
                return false;
            for (u64 i = 0; i < a; i++) {
                if (!r.byte(v))
                    return false;
                if (i)
                    text.put(' ');
                text.put(type_name(v));
            }
            return true;
        case OpKind::REF_NULL:
            // The heap type is in the mnemonic.
            if (!r.uleb(a))
                return false;
            if (a == 0x70)
                text.put("_func");
            else if (a == 0x6f)
                text.put("_extern");
            else if (a == 0x69)
                text.put("_exn");
            else
                return false;
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

usize decode(Bytes b, Flow &flow, Out &text, Out &notes, bool &ok)
{
    Decoder d{ Reader{ b }, flow, text, notes, Vec<u64>() };
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
    text.put('\t');
    if (info) {
        text.put(info->text);
        ok = d.operands(*info);
    }
    if (!ok) {
        text.clear();
        notes.clear();
        text.put("\t<unknown>");
    }
    return d.r.at;
}
