// Arbitrary-precision integers: the magnitude arithmetic, and the integer arm
// of the number tower over it.
//
// Limbs are u32 and the intermediates u64, which wasm has natively. A wider
// limb would want 128-bit division, and that is a compiler-rt call this target
// cannot link.
#include "bigint.h"

#include "gc.h"
#include "kernel/fmt.h"
#include "kernel/vec.h"
#include "math/math.h"
#include "ops.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

// A scratch magnitude. Everything below works on these and only the last step
// makes an object, so no allocation of ours is ever a collection point in the
// middle of a sum.
using Mag = Vec<u32>;

void trim(Mag &m)
{
    while (m.size() > 1 && m[m.size() - 1] == 0)
        m.pop();
    if (!m.size())
        (void)m.push(0);
}

bool is_zero(const Mag &m)
{
    return m.size() == 1 && m[0] == 0;
}

// -1, 0 or 1 by magnitude alone.
int cmp_mag(const u32 *a, usize na, const u32 *b, usize nb)
{
    while (na > 1 && a[na - 1] == 0)
        na--;
    while (nb > 1 && b[nb - 1] == 0)
        nb--;
    if (na != nb)
        return na < nb ? -1 : 1;
    for (usize i = na; i-- > 0;)
        if (a[i] != b[i])
            return a[i] < b[i] ? -1 : 1;
    return 0;
}

bool add_mag(const u32 *a, usize na, const u32 *b, usize nb, Mag &out)
{
    usize n   = na > nb ? na : nb;
    u64 carry = 0;
    for (usize i = 0; i < n; i++) {
        u64 s = carry + (i < na ? a[i] : 0) + (i < nb ? b[i] : 0);
        if (!out.push(u32(s)))
            return false;
        carry = s >> 32;
    }
    if (carry && !out.push(u32(carry)))
        return false;
    trim(out);
    return true;
}

// a - b, the caller having checked that a is the larger.
bool sub_mag(const u32 *a, usize na, const u32 *b, usize nb, Mag &out)
{
    i64 borrow = 0;
    for (usize i = 0; i < na; i++) {
        i64 d  = i64(a[i]) - i64(i < nb ? b[i] : 0) - borrow;
        borrow = d < 0;
        if (borrow)
            d += i64(1) << 32;
        if (!out.push(u32(d)))
            return false;
    }
    trim(out);
    return true;
}

bool mul_mag(const u32 *a, usize na, const u32 *b, usize nb, Mag &out)
{
    if (!out.resize(na + nb))
        return false;
    for (usize i = 0; i < na + nb; i++)
        out[i] = 0;
    for (usize i = 0; i < na; i++) {
        if (!a[i])
            continue;
        u64 carry = 0;
        for (usize j = 0; j < nb; j++) {
            u64 cur    = u64(out[i + j]) + u64(a[i]) * u64(b[j]) + carry;
            out[i + j] = u32(cur);
            carry      = cur >> 32;
        }
        usize k = i + nb;
        while (carry) {
            u64 cur = u64(out[k]) + carry;
            out[k]  = u32(cur);
            carry   = cur >> 32;
            k++;
        }
    }
    trim(out);
    return true;
}

// One limb in, quotient out, remainder returned.
u32 divmod_small(const u32 *a, usize na, u32 d, Mag &q)
{
    u64 rem = 0;
    for (usize i = na; i-- > 0;) {
        u64 cur = (rem << 32) | a[i];
        q[i]    = u32(cur / d);
        rem     = cur % d;
    }
    return u32(rem);
}

// Knuth's algorithm D. `q` and `r` come back trimmed; b must not be zero.
bool divmod_mag(const u32 *a, usize na, const u32 *b, usize nb, Mag &q, Mag &r)
{
    while (na > 1 && a[na - 1] == 0)
        na--;
    while (nb > 1 && b[nb - 1] == 0)
        nb--;

    if (cmp_mag(a, na, b, nb) < 0) {
        if (!q.push(0))
            return false;
        for (usize i = 0; i < na; i++)
            if (!r.push(a[i]))
                return false;
        trim(r);
        return true;
    }

    if (nb == 1) {
        if (!q.resize(na))
            return false;
        u32 rem = divmod_small(a, na, b[0], q);
        trim(q);
        return r.push(rem);
    }

    // Normalise so the divisor's top limb has its high bit set, which is what
    // makes the two-limb estimate off by at most one.
    u32 shift = 0;
    while (!((b[nb - 1] << shift) & 0x80000000u))
        shift++;

    Mag u, v;
    if (!u.resize(na + 1) || !v.resize(nb))
        return false;
    u32 carry = 0;
    for (usize i = 0; i < na; i++) {
        u[i]  = (a[i] << shift) | carry;
        carry = shift ? u32(u64(a[i]) >> (32 - shift)) : 0;
    }
    u[na] = carry;
    carry = 0;
    for (usize i = 0; i < nb; i++) {
        v[i]  = (b[i] << shift) | carry;
        carry = shift ? u32(u64(b[i]) >> (32 - shift)) : 0;
    }

    if (!q.resize(na - nb + 1))
        return false;
    for (usize i = 0; i < q.size(); i++)
        q[i] = 0;

    u32 top = v[nb - 1], next = v[nb - 2];
    for (usize j = na - nb + 1; j-- > 0;) {
        u64 head = (u64(u[j + nb]) << 32) | u[j + nb - 1];
        u64 qhat = head / top;
        u64 rhat = head % top;
        while (qhat > 0xffffffffu || qhat * next > ((rhat << 32) | u[j + nb - 2])) {
            qhat--;
            rhat += top;
            if (rhat > 0xffffffffu)
                break;
        }

        // Multiply and subtract; add the divisor back on the rare overshoot.
        i64 borrow = 0;
        u64 mul    = 0;
        for (usize i = 0; i < nb; i++) {
            mul    = qhat * v[i] + mul;
            i64 d  = i64(u[i + j]) - i64(u32(mul)) - borrow;
            borrow = d < 0;
            if (borrow)
                d += i64(1) << 32;
            u[i + j] = u32(d);
            mul >>= 32;
        }
        i64 d  = i64(u[j + nb]) - i64(u32(mul)) - borrow;
        borrow = d < 0;
        if (borrow)
            d += i64(1) << 32;
        u[j + nb] = u32(d);

        if (borrow) {
            qhat--;
            u64 back = 0;
            for (usize i = 0; i < nb; i++) {
                back     = back + u[i + j] + v[i];
                u[i + j] = u32(back);
                back >>= 32;
            }
            u[j + nb] = u32(u64(u[j + nb]) + back);
        }
        q[j] = u32(qhat);
    }
    trim(q);

    // Undo the normalisation on the remainder.
    if (!r.resize(nb))
        return false;
    for (usize i = 0; i < nb; i++) {
        u32 hi = shift && i + 1 < nb ? u32(u64(u[i + 1]) << (32 - shift)) : 0;
        r[i]   = (u[i] >> shift) | hi;
    }
    trim(r);
    return true;
}

// ------------------------------------------------------------ the value side

// The magnitude and sign of any integer, copied out so the caller may hold it
// across an allocation.
bool spread(Value v, Mag &m, bool &neg)
{
    i64 n = 0;
    if (!is_big(v) && as_index(v, n)) {
        neg   = n < 0;
        u64 u = neg ? u64(-(n + 1)) + 1 : u64(n);
        if (!m.push(u32(u)))
            return false;
        return m.push(u32(u >> 32)) && (trim(m), true);
    }
    BigObj *b = big_of(v);
    neg       = b->neg;
    for (u32 i = 0; i < b->len; i++)
        if (!m.push(b->limbs()[i]))
            return false;
    return true;
}

Value from_mag(const Mag &m, bool neg)
{
    return big_make(m.data(), m.size(), neg);
}

usize bit_length(const Mag &m)
{
    usize n = m.size() * 32;
    u32 t   = m[m.size() - 1];
    if (!t)
        return 0;
    while (!(t & 0x80000000u)) {
        t <<= 1;
        n--;
    }
    return n;
}

R bitwise(Value a, Value b, Op op, Value &out)
{
    Mag ma, mb;
    bool na = false, nb = false;
    if (!spread(a, ma, na) || !spread(b, mb, nb))
        return oom();

    usize n = (ma.size() > mb.size() ? ma.size() : mb.size()) + 1;

    // The two's-complement limbs, made once rather than bit by bit.
    Mag ta, tb;
    if (!ta.resize(n) || !tb.resize(n))
        return oom();
    u64 borrow_a = na ? 1 : 0, borrow_b = nb ? 1 : 0;
    for (usize i = 0; i < n; i++) {
        u32 wa = i < ma.size() ? ma[i] : 0;
        if (na) {
            u64 d    = u64(wa) - borrow_a;
            borrow_a = d >> 63;
            ta[i]    = ~u32(d);
        } else {
            ta[i] = wa;
        }
        u32 wb = i < mb.size() ? mb[i] : 0;
        if (nb) {
            u64 d    = u64(wb) - borrow_b;
            borrow_b = d >> 63;
            tb[i]    = ~u32(d);
        } else {
            tb[i] = wb;
        }
    }

    Mag tr;
    if (!tr.resize(n))
        return oom();
    for (usize i = 0; i < n; i++)
        tr[i] = op == Op::And ? (ta[i] & tb[i]) : op == Op::Or ? (ta[i] | tb[i]) : (ta[i] ^ tb[i]);

    bool neg = (tr[n - 1] & 0x80000000u) != 0;
    if (neg) {
        // Back to a magnitude: ~r + 1.
        u64 carry = 1;
        for (usize i = 0; i < n; i++) {
            u64 s = u64(~tr[i]) + carry;
            tr[i] = u32(s);
            carry = s >> 32;
        }
    }
    trim(tr);
    out = from_mag(tr, neg);
    return out.is_nil() ? R::Err : R::Ok;
}

R shift_left(Value a, i64 by, Value &out)
{
    Mag m;
    bool neg = false;
    if (!spread(a, m, neg))
        return oom();
    if (is_zero(m)) {
        out = Value::of_int(0);
        return R::Ok;
    }
    usize whole = usize(by / 32), bits = usize(by % 32);
    Mag r;
    if (!r.resize(m.size() + whole + 1))
        return oom();
    for (usize i = 0; i < r.size(); i++)
        r[i] = 0;
    for (usize i = 0; i < m.size(); i++) {
        u64 v = u64(m[i]) << bits;
        r[i + whole] |= u32(v);
        r[i + whole + 1] |= u32(v >> 32);
    }
    trim(r);
    out = from_mag(r, neg);
    return out.is_nil() ? R::Err : R::Ok;
}

R shift_right(Value a, i64 by, Value &out)
{
    Mag m;
    bool neg = false;
    if (!spread(a, m, neg))
        return oom();
    usize whole = usize(by / 32), bits = usize(by % 32);

    // A negative shifts toward minus infinity, so anything shifted out of a
    // negative number rounds it one further down.
    bool lost = false;
    for (usize i = 0; i < whole && i < m.size(); i++)
        if (m[i])
            lost = true;
    if (whole < m.size() && bits && (m[whole] & ((u32(1) << bits) - 1)))
        lost = true;

    Mag r;
    if (whole >= m.size()) {
        if (!r.push(0))
            return oom();
    } else {
        for (usize i = whole; i < m.size(); i++) {
            u64 v = m[i] >> bits;
            if (bits && i + 1 < m.size())
                v |= u64(m[i + 1]) << (32 - bits);
            if (!r.push(u32(v)))
                return oom();
        }
        trim(r);
    }

    if (neg && lost) {
        Mag one, sum;
        if (!one.push(1) || !add_mag(r.data(), r.size(), one.data(), 1, sum))
            return oom();
        out = from_mag(sum, true);
    } else {
        out = from_mag(r, neg && !is_zero(r));
    }
    return out.is_nil() ? R::Err : R::Ok;
}

// The quotient and remainder Python means: the quotient floors and the
// remainder takes the divisor's sign.
R floor_divmod(Value a, Value b, Value *q, Value *r)
{
    Mag ma, mb;
    bool na = false, nb = false;
    if (!spread(a, ma, na) || !spread(b, mb, nb))
        return oom();
    if (is_zero(mb))
        return err_set("ZeroDivisionError", "integer division or modulo by zero");

    Mag mq, mr;
    if (!divmod_mag(ma.data(), ma.size(), mb.data(), mb.size(), mq, mr))
        return oom();

    bool qneg = na != nb;
    if (qneg && !is_zero(mr)) {
        // Truncation gave the wrong answer by one; floor it and fold the
        // remainder back over the divisor.
        Mag one, mq2, mr2;
        if (!one.push(1) || !add_mag(mq.data(), mq.size(), one.data(), 1, mq2))
            return oom();
        if (!sub_mag(mb.data(), mb.size(), mr.data(), mr.size(), mr2))
            return oom();
        mq = static_cast<Mag &&>(mq2);
        mr = static_cast<Mag &&>(mr2);
    }
    if (q) {
        *q = from_mag(mq, qneg && !is_zero(mq));
        if (q->is_nil())
            return R::Err;
    }
    if (r) {
        Root keep{ q ? *q : Value() };
        *r = from_mag(mr, nb && !is_zero(mr));
        if (r->is_nil())
            return R::Err;
        if (q)
            *q = keep.v;
    }
    return R::Ok;
}

} // namespace

// ------------------------------------------------------------- construction

Value big_make(const u32 *limbs, usize len, bool neg)
{
    while (len > 1 && limbs[len - 1] == 0)
        len--;
    if (!len)
        return Value::of_int(0);

    // Small if it fits, always: two ints that are equal are the same shape.
    if (len <= 2) {
        u64 m = limbs[0] | (len > 1 ? u64(limbs[1]) << 32 : 0);
        if (!neg && m <= u64(Value::SMALL_MAX))
            return Value::of_int(i32(m));
        if (neg && m <= u64(-i64(Value::SMALL_MIN)))
            return Value::of_int(i32(-i64(m)));
    }

    BigObj *o = static_cast<BigObj *>(obj_alloc(&int_type, sizeof(BigObj) + len * sizeof(u32)));
    if (!o)
        return oom(), Value();
    o->len = u32(len);
    o->neg = neg;
    for (usize i = 0; i < len; i++)
        o->limbs()[i] = limbs[i];
    return obj_value(o);
}

Value big_from_i64(i64 n)
{
    if (Value::fits_small(n))
        return Value::of_int(i32(n));
    bool neg = n < 0;
    u64 m    = neg ? u64(-(n + 1)) + 1 : u64(n);
    u32 two[2]{ u32(m), u32(m >> 32) };
    return big_make(two, 2, neg);
}

Value big_of_value(Value v)
{
    if (is_big(v))
        return v;
    i64 n = 0;
    return as_index(v, n) ? big_from_i64(n) : Value();
}

// Self-contained: as_index answers through this one, so asking it back would
// be a loop.
bool int_to_i64(Value v, i64 &out)
{
    if (v.is_int()) {
        out = v.as_int();
        return true;
    }
    if (is_bool(v)) {
        out = is_true(v) ? 1 : 0;
        return true;
    }
    if (!is_big(v))
        return false;
    BigObj *b = big_of(v);
    if (b->len > 2)
        return false;
    u64 m = b->limbs()[0] | (b->len > 1 ? u64(b->limbs()[1]) << 32 : 0);
    if (b->neg) {
        if (m > u64(1) << 63)
            return false;
        out = m == (u64(1) << 63) ? i64(-9223372036854775807LL - 1) : -i64(m);
        return true;
    }
    if (m > 0x7fffffffffffffffULL)
        return false;
    out = i64(m);
    return true;
}

f64 int_to_f64(Value v)
{
    i64 n = 0;
    if (!is_big(v) && as_index(v, n))
        return f64(n);
    BigObj *b = big_of(v);
    f64 x     = 0;
    for (usize i = b->len; i-- > 0;)
        x = x * 4294967296.0 + f64(b->limbs()[i]);
    return b->neg ? -x : x;
}

Value int_from_f64(f64 x)
{
    bool neg = x < 0;
    x        = neg ? -x : x;
    x        = trunc(x);
    if (x < 2147483648.0)
        return big_from_i64(neg ? -i64(x) : i64(x));

    // Peel 32 bits at a time off the top, which is exact for an f64.
    i32 e = 0;
    frexp(x, &e);
    usize len = usize((e + 31) / 32);
    Mag m;
    if (!m.resize(len))
        return oom(), Value();
    for (usize i = 0; i < len; i++)
        m[i] = 0;
    f64 rest = x;
    for (usize i = len; i-- > 0;) {
        f64 scale = ldexp(1.0, i32(i) * 32);
        f64 q     = trunc(rest / scale);
        m[i]      = u32(q);
        rest -= q * scale;
    }
    return big_make(m.data(), m.size(), neg);
}

// ---------------------------------------------------------------- the slots

bool int_truth_of(Value v)
{
    return v.is_int() ? v.as_int() != 0 : true; // a big is never zero
}

u32 int_hash_of(Value v)
{
    // CPython's numeric hash for a 32-bit Py_hash_t: the magnitude modulo
    // 2**31 - 1, the sign kept, and -1 said as -2.
    u64 x    = 0;
    bool neg = false;
    i64 n    = 0;
    if (!is_big(v) && as_index(v, n)) {
        neg = n < 0;
        x   = (neg ? u64(-(n + 1)) + 1 : u64(n)) % HASH_MODULUS;
    } else {
        BigObj *b = big_of(v);
        neg       = b->neg;
        for (usize i = b->len; i-- > 0;)
            x = ((x << 32) | b->limbs()[i]) % HASH_MODULUS;
    }
    u32 h = neg ? u32(0u - u32(x)) : u32(x);
    return h == u32(-1) ? u32(-2) : h;
}

usize int_bits(Value v)
{
    Mag m;
    bool neg = false;
    if (!spread(v, m, neg))
        return oom(), 0;
    return bit_length(m);
}

usize int_ones(Value v)
{
    Mag m;
    bool neg = false;
    if (!spread(v, m, neg))
        return oom(), 0;
    usize n = 0;
    for (usize i = 0; i < m.size(); i++)
        for (u32 w = m[i]; w; w >>= 1)
            n += w & 1;
    return n;
}

R int_digits(Value v, u32 base, bool upper, String &out)
{
    const char *D =
        upper ? "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ" : "0123456789abcdefghijklmnopqrstuvwxyz";
    Mag m;
    bool neg = false;
    if (!spread(v, m, neg))
        return oom();
    if (is_zero(m))
        return out.push('0') ? R::Ok : oom();

    // Digits come out least significant first, so they are reversed at the
    // end. A power of two takes the shift path and needs no division at all.
    String rev;
    if (base == 2 || base == 4 || base == 8 || base == 16 || base == 32) {
        u32 bits    = base == 2 ? 1 : base == 4 ? 2 : base == 8 ? 3 : base == 16 ? 4 : 5;
        u32 mask    = base - 1;
        usize total = m.size() * 32;
        for (usize at = 0; at < total; at += bits) {
            usize i = at / 32, off = at % 32;
            u32 d = (m[i] >> off) & mask;
            if (off + bits > 32 && i + 1 < m.size())
                d |= (m[i + 1] << (32 - off)) & mask;
            if (!rev.push(D[d]))
                return oom();
        }
        while (rev.size() > 1 && rev.str()[rev.size() - 1] == '0')
            rev.pop();
    } else {
        // Divide by the largest power of the base that fits a limb, so one
        // long division yields several digits.
        u32 chunk = base, per = 1;
        while (u64(chunk) * base < 0x100000000ULL) {
            chunk *= base;
            per++;
        }
        Mag cur;
        for (usize i = 0; i < m.size(); i++)
            if (!cur.push(m[i]))
                return oom();
        while (!is_zero(cur)) {
            Mag q;
            if (!q.resize(cur.size()))
                return oom();
            u32 rem = divmod_small(cur.data(), cur.size(), chunk, q);
            trim(q);
            bool last = is_zero(q);
            for (u32 k = 0; k < per; k++) {
                if (last && !rem && k)
                    break;
                if (!rev.push(D[rem % base]))
                    return oom();
                rem /= base;
            }
            cur = static_cast<Mag &&>(q);
        }
    }
    for (usize i = rev.size(); i-- > 0;)
        if (!out.push(rev.str()[i]))
            return oom();
    return R::Ok;
}

// --------------------------------------------------------------- arithmetic

R int_arith(Value a, Value b, Op op, Value &out)
{
    // The ordinary machine path, which is where all but a handful of programs
    // stay. A checked 64-bit multiply would be a 128-bit one underneath and
    // that is a compiler-rt call this target cannot link, so the product is
    // taken only where both sides are narrow enough for it to fit.
    i64 x = 0, y = 0;
    if (int_to_i64(a, x) && int_to_i64(b, y)) {
        i64 r   = 0;
        bool ok = false;
        switch (op) {
        case Op::Add:
            ok = !__builtin_add_overflow(x, y, &r);
            break;
        case Op::Sub:
            ok = !__builtin_sub_overflow(x, y, &r);
            break;
        case Op::Mul:
            if (x > -(i64(1) << 31) && x < (i64(1) << 31) && y > -(i64(1) << 31) &&
                y < (i64(1) << 31)) {
                r  = x * y;
                ok = true;
            }
            break;
        default:
            break;
        }
        if (ok) {
            out = big_from_i64(r);
            return out.is_nil() ? R::Err : R::Ok;
        }
    }

    switch (op) {
    case Op::Add:
    case Op::Sub: {
        Mag ma, mb;
        bool na = false, nb = false;
        if (!spread(a, ma, na) || !spread(b, mb, nb))
            return oom();
        if (op == Op::Sub)
            nb = !nb && !is_zero(mb);
        Mag r;
        if (na == nb) {
            if (!add_mag(ma.data(), ma.size(), mb.data(), mb.size(), r))
                return oom();
            out = from_mag(r, na);
        } else if (cmp_mag(ma.data(), ma.size(), mb.data(), mb.size()) >= 0) {
            if (!sub_mag(ma.data(), ma.size(), mb.data(), mb.size(), r))
                return oom();
            out = from_mag(r, na && !is_zero(r));
        } else {
            if (!sub_mag(mb.data(), mb.size(), ma.data(), ma.size(), r))
                return oom();
            out = from_mag(r, nb && !is_zero(r));
        }
        return out.is_nil() ? R::Err : R::Ok;
    }

    case Op::Mul: {
        Mag ma, mb, r;
        bool na = false, nb = false;
        if (!spread(a, ma, na) || !spread(b, mb, nb))
            return oom();
        if (!mul_mag(ma.data(), ma.size(), mb.data(), mb.size(), r))
            return oom();
        out = from_mag(r, (na != nb) && !is_zero(r));
        return out.is_nil() ? R::Err : R::Ok;
    }

    case Op::FloorDiv:
        return floor_divmod(a, b, &out, nullptr);
    case Op::Mod:
        return floor_divmod(a, b, nullptr, &out);
    case Op::Div:
        return int_truediv(a, b, out);
    case Op::Pow:
        return int_power(a, b, Value(), out);

    case Op::And:
    case Op::Or:
    case Op::Xor:
        return bitwise(a, b, op, out);

    case Op::Lsh:
    case Op::Rsh: {
        i64 n = 0;
        if (!int_to_i64(b, n))
            return err_set("OverflowError", "shift count too large");
        if (n < 0)
            return err_set("ValueError", "negative shift count");
        if (op == Op::Lsh && n > (i64(1) << 28))
            return err_set("OverflowError", "shift count too large");
        return op == Op::Lsh ? shift_left(a, n, out) : shift_right(a, n, out);
    }

    default:
        break;
    }
    return R::NotImpl;
}

R int_negate(Value v, Value &out)
{
    Mag m;
    bool neg = false;
    if (!spread(v, m, neg))
        return oom();
    out = from_mag(m, !neg && !is_zero(m));
    return out.is_nil() ? R::Err : R::Ok;
}

R int_absolute(Value v, Value &out)
{
    Mag m;
    bool neg = false;
    if (!spread(v, m, neg))
        return oom();
    out = from_mag(m, false);
    return out.is_nil() ? R::Err : R::Ok;
}

// ~x is -x - 1, which needs no bit work at all.
R int_invert_op(Value v, Value &out)
{
    Root neg;
    if (int_negate(v, neg.v) != R::Ok)
        return R::Err;
    return int_arith(neg.v, Value::of_int(1), Op::Sub, out);
}

// -------------------------------------------------------------- comparison

R int_compare(Value a, Value b, Cmp op, bool &out)
{
    Mag ma, mb;
    bool na = false, nb = false;
    if (!spread(a, ma, na) || !spread(b, mb, nb))
        return oom();
    int c;
    if (na != nb)
        c = na ? -1 : 1;
    else {
        c = cmp_mag(ma.data(), ma.size(), mb.data(), mb.size());
        if (na)
            c = -c;
    }
    switch (op) {
    case Cmp::Eq:
        out = c == 0;
        break;
    case Cmp::Ne:
        out = c != 0;
        break;
    case Cmp::Lt:
        out = c < 0;
        break;
    case Cmp::Le:
        out = c <= 0;
        break;
    case Cmp::Gt:
        out = c > 0;
        break;
    case Cmp::Ge:
        out = c >= 0;
        break;
    default:
        return R::NotImpl;
    }
    return R::Ok;
}

R intfloat_compare(Value i, f64 y, bool flip, Cmp op, bool &out)
{
    // nan is equal to nothing and ordered against nothing.
    if (isnan(y)) {
        if (op == Cmp::Ne) {
            out = true;
            return R::Ok;
        }
        out = false;
        return R::Ok;
    }

    int c; // the integer against the float: -1, 0 or 1
    if (isinf(y)) {
        c = y > 0 ? -1 : 1;
    } else {
        f64 fl = floor(y);
        Root whole{ int_from_f64(fl) };
        if (whole.v.is_nil())
            return R::Err;
        bool less = false, equal = false;
        if (int_compare(i, whole.v, Cmp::Lt, less) != R::Ok ||
            int_compare(i, whole.v, Cmp::Eq, equal) != R::Ok)
            return R::Err;
        // i == floor(y) still leaves the fraction to settle it.
        c = less ? -1 : equal ? (y > fl ? -1 : 0) : 1;
    }
    if (flip)
        c = -c;

    switch (op) {
    case Cmp::Eq:
        out = c == 0;
        break;
    case Cmp::Ne:
        out = c != 0;
        break;
    case Cmp::Lt:
        out = c < 0;
        break;
    case Cmp::Le:
        out = c <= 0;
        break;
    case Cmp::Gt:
        out = c > 0;
        break;
    case Cmp::Ge:
        out = c >= 0;
        break;
    default:
        return R::NotImpl;
    }
    return R::Ok;
}

// ------------------------------------------------------------------- power

R int_power(Value a, Value b, Value mod, Value &out)
{
    if (int_is_neg(b)) {
        if (!mod.is_nil())
            return err_set("ValueError",
                           "pow() 2nd argument cannot be negative when 3rd argument specified");
        f64 x = int_to_f64(a), y = int_to_f64(b);
        out = float_new(pow(x, y));
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (!mod.is_nil()) {
        bool zero = false;
        if (int_compare(mod, Value::of_int(0), Cmp::Eq, zero) != R::Ok)
            return R::Err;
        if (zero)
            return err_set("ValueError", "pow() 3rd argument cannot be 0");
    }

    // 0, 1 and -1 to any power at all, however wide: the answer needs no
    // work and refusing the exponent would be wrong.
    i64 small = 0;
    if (int_to_i64(a, small) && small >= -1 && small <= 1 && mod.is_nil()) {
        bool zero_exp = false;
        if (int_compare(b, Value::of_int(0), Cmp::Eq, zero_exp) != R::Ok)
            return R::Err;
        bool odd = false;
        if (!zero_exp && int_arith(b, Value::of_int(1), Op::And, out) == R::Ok)
            odd = out == Value::of_int(1);
        out = zero_exp     ? Value::of_int(1)
              : small == 0 ? Value::of_int(0)
              : small == 1 ? Value::of_int(1)
                           : Value::of_int(odd ? -1 : 1);
        return R::Ok;
    }

    // Without a modulus the answer is as wide as the base times the exponent,
    // so a big exponent is refused rather than filling the heap. With one it
    // is bounded, and the exponent's own width does not matter: the loop
    // walks its bits.
    if (mod.is_nil()) {
        i64 e = 0;
        if (!int_to_i64(b, e) || e > (i64(1) << 22))
            return err_set("OverflowError", "exponent too large");
    }

    Root base{ a }, acc{ Value::of_int(1) }, m{ mod }, e{ b };
    if (!m.v.is_nil() && int_arith(base.v, m.v, Op::Mod, base.v) != R::Ok)
        return R::Err;
    for (;;) {
        bool done = false;
        if (int_compare(e.v, Value::of_int(0), Cmp::Eq, done) != R::Ok)
            return R::Err;
        if (done)
            break;
        Root bit;
        if (int_arith(e.v, Value::of_int(1), Op::And, bit.v) != R::Ok)
            return R::Err;
        if (bit.v == Value::of_int(1)) {
            if (int_arith(acc.v, base.v, Op::Mul, acc.v) != R::Ok)
                return R::Err;
            if (!m.v.is_nil() && int_arith(acc.v, m.v, Op::Mod, acc.v) != R::Ok)
                return R::Err;
        }
        if (int_arith(e.v, Value::of_int(1), Op::Rsh, e.v) != R::Ok)
            return R::Err;
        if (int_compare(e.v, Value::of_int(0), Cmp::Eq, done) != R::Ok)
            return R::Err;
        if (done)
            break;
        if (int_arith(base.v, base.v, Op::Mul, base.v) != R::Ok)
            return R::Err;
        if (!m.v.is_nil() && int_arith(base.v, m.v, Op::Mod, base.v) != R::Ok)
            return R::Err;
    }
    // A zero exponent skipped the loop, leaving the 1 unreduced.
    if (!m.v.is_nil() && int_arith(acc.v, m.v, Op::Mod, acc.v) != R::Ok)
        return R::Err;
    out = acc.v;
    return R::Ok;
}

// ------------------------------------------------------------ true division

R int_truediv(Value a, Value b, Value &out)
{
    bool zero = false;
    if (int_compare(b, Value::of_int(0), Cmp::Eq, zero) != R::Ok)
        return R::Err;
    if (zero)
        return err_set("ZeroDivisionError", "division by zero");

    // Both exact as doubles: one machine divide, rounded once by the machine.
    i64 x = 0, y = 0;
    if (int_to_i64(a, x) && int_to_i64(b, y) && x > -(i64(1) << 53) && x < (i64(1) << 53) &&
        y > -(i64(1) << 53) && y < (i64(1) << 53)) {
        out = float_new(f64(x) / f64(y));
        return out.is_nil() ? R::Err : R::Ok;
    }

    Mag ma, mb;
    bool na = false, nb = false;
    if (!spread(a, ma, na) || !spread(b, mb, nb))
        return oom();
    if (is_zero(ma)) {
        out = float_new(na != nb ? -0.0 : 0.0);
        return out.is_nil() ? R::Err : R::Ok;
    }

    // Scale so the quotient keeps 55 bits -- two more than a double holds --
    // and divide once. A non-zero remainder then becomes a sticky low bit, so
    // the single conversion of the quotient rounds the way the whole division
    // would. Converting each side to a double and dividing would round three
    // times instead.
    i64 shift = 55 + i64(bit_length(mb)) - i64(bit_length(ma));
    Root num{ from_mag(ma, false) }, den{ from_mag(mb, false) };
    if (num.v.is_nil() || den.v.is_nil())
        return R::Err;
    if (shift > 0) {
        if (shift_left(num.v, shift, num.v) != R::Ok)
            return R::Err;
    } else if (shift < 0) {
        if (shift_left(den.v, -shift, den.v) != R::Ok)
            return R::Err;
    }

    Root q, r;
    if (floor_divmod(num.v, den.v, &q.v, &r.v) != R::Ok)
        return R::Err;
    bool exact = false;
    if (int_compare(r.v, Value::of_int(0), Cmp::Eq, exact) != R::Ok)
        return R::Err;
    if (!exact && int_arith(q.v, Value::of_int(1), Op::Or, q.v) != R::Ok)
        return R::Err;

    f64 val = ldexp(int_to_f64(q.v), i32(-shift));
    if (na != nb)
        val = -val;
    out = float_new(val);
    return out.is_nil() ? R::Err : R::Ok;
}

// ------------------------------------------------------------------- octets

R int_to_octets(Value v, usize n, bool little, bool sign, String &out)
{
    Mag m;
    bool neg = false;
    if (!spread(v, m, neg))
        return oom();
    if (neg && !sign)
        return err_set("OverflowError", "can\'t convert negative int to unsigned");

    // The two\'s-complement octets, n of them, and then the check that the
    // value really fits: what came out has to read back as what went in.
    Vec<u8> bytes;
    if (!bytes.resize(n ? n : 1))
        return oom();
    for (usize i = 0; i < n; i++) {
        usize limb = i / 4;
        bytes[i]   = limb < m.size() ? u8(m[limb] >> ((i % 4) * 8)) : 0;
    }
    if (neg) {
        u32 carry = 1;
        for (usize i = 0; i < n; i++) {
            u32 s2   = u32(u8(~bytes[i])) + carry;
            bytes[i] = u8(s2);
            carry    = s2 >> 8;
        }
    }

    Root back{ int_from_octets(Str(reinterpret_cast<char *>(bytes.data()), n), true, sign) };
    if (back.v.is_nil())
        return R::Err;
    bool same = false;
    if (int_compare(back.v, v, Cmp::Eq, same) != R::Ok)
        return R::Err;
    if (!same)
        return err_set("OverflowError", "int too big to convert");

    for (usize i = 0; i < n; i++)
        if (!out.push(char(bytes[little ? i : n - 1 - i])))
            return oom();
    return R::Ok;
}

Value int_from_octets(Str s, bool little, bool sign)
{
    usize n = s.size();
    Vec<u8> bytes;
    if (!bytes.resize(n ? n : 1))
        return oom(), Value();
    for (usize i = 0; i < n; i++)
        bytes[i] = u8(s[little ? i : n - 1 - i]);
    if (!n)
        return Value::of_int(0);

    bool neg = sign && (bytes[n - 1] & 0x80);
    if (neg) {
        u32 carry = 1;
        for (usize i = 0; i < n; i++) {
            u32 d    = u32(u8(~bytes[i])) + carry;
            bytes[i] = u8(d);
            carry    = d >> 8;
        }
    }
    Mag m;
    if (!m.resize((n + 3) / 4))
        return oom(), Value();
    for (usize i = 0; i < m.size(); i++)
        m[i] = 0;
    for (usize i = 0; i < n; i++)
        m[i / 4] |= u32(bytes[i]) << ((i % 4) * 8);
    trim(m);
    return big_make(m.data(), m.size(), neg && !is_zero(m));
}

// ------------------------------------------------------------- float ratio

R float_ratio(f64 x, Value &num, Value &den)
{
    if (isnan(x))
        return err_set("ValueError", "cannot convert NaN to integer ratio");
    if (isinf(x))
        return err_set("OverflowError", "cannot convert Infinity to integer ratio");
    if (x == 0) {
        num = Value::of_int(0);
        den = Value::of_int(1);
        return R::Ok;
    }
    // Every float is m * 2**e with m a 53-bit integer: scale until it is one.
    i32 e = 0;
    f64 m = frexp(x, &e);
    for (int i = 0; i < 300 && m != trunc(m); i++) {
        m *= 2;
        e--;
    }
    Root n{ int_from_f64(m) };
    if (n.v.is_nil())
        return R::Err;
    Root d{ Value::of_int(1) };
    if (e > 0) {
        if (shift_left(n.v, e, n.v) != R::Ok)
            return R::Err;
    } else if (e < 0) {
        if (shift_left(d.v, -e, d.v) != R::Ok)
            return R::Err;
    }
    num = n.v;
    den = d.v;
    return R::Ok;
}

// ------------------------------------------------------------------ parsing

Value int_parse(Str s, u32 base)
{
    usize i = 0, j = s.size();
    while (i < j && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r' || s[i] == '\f' ||
                     s[i] == '\v'))
        i++;
    while (j > i && (s[j - 1] == ' ' || s[j - 1] == '\t' || s[j - 1] == '\n' || s[j - 1] == '\r' ||
                     s[j - 1] == '\f' || s[j - 1] == '\v'))
        j--;

    bool neg = false;
    if (i < j && (s[i] == '+' || s[i] == '-'))
        neg = s[i++] == '-';

    if ((base == 0 || base == 16 || base == 2 || base == 8) && i + 1 < j && s[i] == '0') {
        char c   = s[i + 1] | 0x20;
        u32 want = c == 'x' ? 16 : c == 'o' ? 8 : c == 'b' ? 2 : 0;
        if (want && (base == 0 || base == want)) {
            base = want;
            i += 2;
        }
    }
    // Base 0 reads the prefix and otherwise means decimal -- but a leading
    // zero was octal once, so it is refused rather than quietly read as ten.
    bool bare = base == 0;
    if (base == 0)
        base = 10;
    if (bare && i < j && s[i] == '0') {
        for (usize k = i; k < j; k++)
            if (s[k] != '0' && s[k] != '_')
                return err_set("ValueError", "invalid literal for int()"), Value();
    }
    if (base < 2 || base > 36)
        return err_set("ValueError", "int() base must be >= 2 and <= 36, or 0"), Value();

    Mag m;
    if (!m.push(0))
        return oom(), Value();
    bool any = false, sep = true;
    for (; i < j; i++) {
        char c = s[i];
        if (c == '_') {
            if (sep)
                return err_set("ValueError", "invalid literal for int()"), Value();
            sep = true;
            continue;
        }
        u32 d;
        if (c >= '0' && c <= '9')
            d = u32(c - '0');
        else if ((c | 0x20) >= 'a' && (c | 0x20) <= 'z')
            d = u32((c | 0x20) - 'a') + 10;
        else
            return err_set("ValueError", "invalid literal for int()"), Value();
        if (d >= base)
            return err_set("ValueError", "invalid literal for int()"), Value();

        u64 carry = d;
        for (usize k = 0; k < m.size(); k++) {
            u64 cur = u64(m[k]) * base + carry;
            m[k]    = u32(cur);
            carry   = cur >> 32;
        }
        if (carry && !m.push(u32(carry)))
            return oom(), Value();
        any = true;
        sep = false;
    }
    if (!any || sep)
        return err_set("ValueError", "invalid literal for int()"), Value();
    trim(m);
    return big_make(m.data(), m.size(), neg && !is_zero(m));
}
