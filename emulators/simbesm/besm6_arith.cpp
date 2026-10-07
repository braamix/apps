// BESM-6 arithmetic instructions.
//
// Copyright (c) 1997-2009, Leonid Broukhis
// Copyright (c) 2009, Serge Vakulenko
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
// SERGE VAKULENKO OR LEONID BROUKHIS BE LIABLE FOR ANY CLAIM, DAMAGES
// OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
// OR OTHER DEALINGS IN THE SOFTWARE.
//
// Except as contained in this notice, the name of Leonid Broukhis or
// Serge Vakulenko shall not be used in advertising or otherwise to promote
// the sale, use or other dealings in this Software without prior written
// authorization from Leonid Broukhis and Serge Vakulenko.
#include "besm6_defs.h"

typedef struct {
    uint64_t mantissa;
    unsigned exponent; // offset by 64
} alureg_t;            // ALU register type

static alureg_t toalu(value_t val)
{
    alureg_t ret;

    ret.mantissa = val & BITS41;
    ret.exponent = (val >> 41) & BITS(7);
    if (ret.mantissa & BIT41)
        ret.mantissa |= BIT42;
    return ret;
}

static inline int is_negative(alureg_t *word)
{
    return (word->mantissa & BIT41) != 0;
}

static void negate(alureg_t *val)
{
    if (is_negative(val))
        val->mantissa |= BIT42;
    val->mantissa = (~val->mantissa + 1) & BITS42;
    if (((val->mantissa >> 1) ^ val->mantissa) & BIT41) {
        val->mantissa >>= 1;
        ++val->exponent;
    }
    if (is_negative(val))
        val->mantissa |= BIT42;
}

// Bit 48 -> 1, bit 47 -> 2 and so on.
// A one in bit 1, and a zero word, both -> 48,
// as in the original version of the instruction set.
int besm6_highest_bit(value_t val)
{
    int n = 32, cnt = 0;
    do {
        value_t tmp = val;
        if (tmp >>= n) {
            cnt += n;
            val = tmp;
        }
    } while (n >>= 1);
    return 48 - cnt;
}

// Normalization and rounding.
// The result goes to ACC and to bits 40-1 of RMR.
// Bits 48-41 of RMR are preserved.
static status_t normalize_and_round(alureg_t acc, uint64_t mr, int rnd_rq)
{
    uint64_t rr = 0;
    int i;
    uint64_t r;

    if (RAU & RAU_NORM_DISABLE)
        goto chk_rnd;
    i = (acc.mantissa >> 39) & 3;
    if (i == 0) {
        r = acc.mantissa & BITS40;
        if (r) {
            int cnt = besm6_highest_bit(r) - 9;
            r <<= cnt;
            rr           = mr >> (40 - cnt);
            acc.mantissa = r | rr;
            mr <<= cnt;
            acc.exponent -= cnt;
            goto chk_zero;
        }
        r = mr & BITS40;
        if (r) {
            int cnt = besm6_highest_bit(r) - 9;
            rr      = mr;
            r <<= cnt;
            acc.mantissa = r;
            mr           = 0;
            acc.exponent -= 40 + cnt;
            goto chk_zero;
        }
        goto zero;
    } else if (i == 3) {
        r = ~acc.mantissa & BITS40;
        if (r) {
            int cnt      = besm6_highest_bit(r) - 9;
            r            = (r << cnt) | ((1LL << cnt) - 1);
            rr           = mr >> (40 - cnt);
            acc.mantissa = BIT41 | (~r & BITS40) | rr;
            mr <<= cnt;
            acc.exponent -= cnt;
            goto chk_zero;
        }
        r = ~mr & BITS40;
        if (r) {
            int cnt      = besm6_highest_bit(r) - 9;
            rr           = mr;
            r            = (r << cnt) | ((1LL << cnt) - 1);
            acc.mantissa = BIT41 | (~r & BITS40);
            mr           = 0;
            acc.exponent -= 40 + cnt;
            goto chk_zero;
        } else {
            rr           = 1;
            acc.mantissa = BIT41;
            mr           = 0;
            acc.exponent -= 80;
            goto chk_zero;
        }
    }
chk_zero:
    if (rr)
        rnd_rq = 0;
chk_rnd:
    if (acc.exponent & 0x8000)
        goto zero;
    if (!(RAU & RAU_ROUND_DISABLE) && rnd_rq)
        acc.mantissa |= 1;

    if (!acc.mantissa && !(RAU & RAU_NORM_DISABLE)) {
    zero:
        ACC = 0;
        RMR &= ~BITS40;
        return SCPE_OK;
    }

    ACC = (value_t)(acc.exponent & BITS(7)) << 41 | (acc.mantissa & BITS41);
    RMR = (RMR & ~BITS40) | (mr & BITS40);
    // On overflow the mantissa and the low bits of the exponent are correct
    if (acc.exponent & 0x80) {
        if (!(RAU & RAU_OVF_DISABLE))
            return STOP_OVFL;
    }
    return SCPE_OK;
}

// Addition and every variant of subtraction.
// Operands: register ACC and the argument 'val'.
// The result goes to ACC and to bits 40-1 of RMR.
status_t besm6_add(value_t val, int negate_acc, int negate_val)
{
    uint64_t mr;
    alureg_t acc, word, a1, a2;
    int diff, neg, rnd_rq = 0;

    acc  = toalu(ACC);
    word = toalu(val);
    if (!negate_acc) {
        if (!negate_val) {
            // Addition
        } else {
            // Subtraction
            negate(&word);
        }
    } else {
        if (!negate_val) {
            // Reverse subtraction
            negate(&acc);
        } else {
            // Subtraction of magnitudes
            if (is_negative(&acc))
                negate(&acc);
            if (!is_negative(&word))
                negate(&word);
        }
    }

    diff = acc.exponent - word.exponent;
    if (diff < 0) {
        diff = -diff;
        a1   = acc;
        a2   = word;
    } else {
        a1 = word;
        a2 = acc;
    }
    mr  = 0;
    neg = is_negative(&a1);
    if (diff == 0) {
        // Nothing to do.
    } else if (diff <= 40) {
        rnd_rq      = (mr = (a1.mantissa << (40 - diff)) & BITS40) != 0;
        a1.mantissa = ((a1.mantissa >> diff) | (neg ? (~0ll << (40 - diff)) : 0)) & BITS42;
    } else if (diff <= 80) {
        diff -= 40;
        rnd_rq = a1.mantissa != 0;
        mr     = ((a1.mantissa >> diff) | (neg ? (~0ll << (40 - diff)) : 0)) & BITS40;
        if (neg) {
            a1.mantissa = BITS42;
        } else
            a1.mantissa = 0;
    } else {
        rnd_rq = a1.mantissa != 0;
        if (neg) {
            mr          = BITS40;
            a1.mantissa = BITS42;
        } else
            mr = a1.mantissa = 0;
    }
    acc.exponent = a2.exponent;
    acc.mantissa = a1.mantissa + a2.mantissa;

    // When normalization to the right is needed, bits 42:41
    // take the value 01 or 10.
    switch ((acc.mantissa >> 40) & 3) {
    case 2:
    case 1:
        rnd_rq |= acc.mantissa & 1;
        mr = (mr >> 1) | ((acc.mantissa & 1) << 39);
        acc.mantissa >>= 1;
        ++acc.exponent;
    }
    return normalize_and_round(acc, mr, rnd_rq);
}

// Mantissa division as the arithmetic unit performs it (TO IY1 700 000 TO-3,
// part IV, section 4.16, sheets 86-102). Non-restoring division, but the
// remainder is kept in carry-save form (sums and carries) and the carries are
// not resolved; only the high bits 41-39 are (RSUD, RPUD, RUD). From them and
// from the previous action (ZU+ChVR) the next action is chosen: subtract the
// divisor, add it, or shift only (table 4.4). The quotient is accumulated as
// two components of 42 bits each (41 plus an extra bit): a positive one (ROM)
// and a negative one (VRU). At the end the quotient is ROM - VRU with a
// special rounding (sheet 102), under which an exact division is always exact.
//
// Mantissas are 42-bit two's complement (two sign bits), the divisor is
// normalized. Result: the quotient mantissa on the same grid, |q| <= 2,
// normalization is up to the caller. Reference model: tools/b6div.py
// in simh-besm6.
#define DBIT(x, n) ((int)((x) >> ((n) - 1)) & 1)
static uint64_t au_divide(uint64_t x, uint64_t y)
{
    const uint64_t lo   = BITS40 >> 2; // carry-save part: bits 38..1
    const uint64_t lo39 = BITS40 >> 1; // bits 39..1
    int yneg            = DBIT(y, 41);
    uint64_t s2, c2, yv, sm, cr, pos, neg, lo_s, lo_c;
    int sud, pud, rud, zu_plus, act, i, a, b, t39, t40;
    int pe, ne, p1, n1, dpr, r1;

    // Remainder R0 = X/2: high bits 41-39 go to RSUD, low bits in
    // single-row code to SM. s2/c2 is the carry-save remainder, already
    // shifted left (bits 39-1); on the first cycle it is X itself.
    s2      = x & lo39;
    c2      = 0;
    sud     = DBIT(x, 42) << 2 | DBIT(x, 41) << 1 | DBIT(x, 40);
    pud     = 0;
    rud     = DBIT(x, 41) << 1 | DBIT(x, 40); // remainder bits 40,39
    zu_plus = !DBIT(x, 41) && yneg;           // sheet 100
    pos = neg = 0;

    for (i = 0; i < 42; i++) {
        // Table 4.4: when the first sign is 0 the action repeats the
        // previous one; otherwise the remainder sign is taken from the
        // carry out of the resolved bits 40,39; code I,II means the sign
        // is unknown, shift only.
        if (!(sud & 4))
            act = zu_plus ? '+' : '-';
        else if ((sud & 3) + pud == 3)
            act = 's';
        else
            act = (((sud & 3) + pud >= 4) != yneg) ? '-' : '+';
        zu_plus = (act == '+');
        pos     = pos << 1 | (act == '-');
        neg     = neg << 1 | (act == '+');

        if (act == 's') {
            // Shift only: RSUD bits 41,40 = 1, bit 39 of the carry-save
            // code is added without the divisor, its carry goes to RPUD40
            // (sheet 98). Bit 38 (carry to RPUD39, as in the circuit of
            // sheet 99) is not mentioned on sheet 98: it was fitted to
            // Dispak's constant E'E-10'.
            a   = DBIT(s2, 39);
            b   = DBIT(c2, 39);
            sud = 6 | (a ^ b);
            pud = (a & b) << 1;
            a   = DBIT(s2, 38);
            b   = DBIT(c2, 38);
            pud |= a & b;
            lo_s = (s2 & (lo >> 1)) | (uint64_t)(a ^ b) << 37;
            lo_c = c2 & (lo >> 1);
        } else {
            // +ChVR is the divisor in direct code, -ChVR is its inverse
            // with a 1 into bit 1
            yv = (act == '+') ? y : ~y & BITS42;
            sm = s2 ^ c2 ^ (yv & lo39);
            cr = ((s2 & c2) | (s2 & yv & lo39) | (c2 & yv & lo39)) << 1;
            if (act == '-')
                cr |= 1;
            // Bit 39 of the sum goes to RSUD, the carries out of 39 and 38
            // to RPUD; bits 41,40 are the shifted RUD plus the divisor.
            t39  = rud & 1;
            t40  = rud >> 1;
            sud  = (t40 ^ DBIT(yv, 41) ^ (t39 & DBIT(yv, 40))) << 2 | (t39 ^ DBIT(yv, 40)) << 1 |
                   DBIT(sm, 39);
            pud  = DBIT(cr, 40) << 1 | DBIT(cr, 39);
            lo_s = sm & lo;
            lo_c = cr & lo;
        }
        // RUD is the resolved bits 40,39 of the new remainder
        rud = ((sud & 3) + pud) & 3;
        s2  = lo_s << 1;
        c2  = lo_c << 1;
    }

    // Quotient = ROM + inverse code of VRU + DPR, plus a rounding 1 in 1rRS2
    pe = pos & 1;
    ne = neg & 1;
    pos >>= 1;
    neg >>= 1;
    p1  = pos & 1;
    n1  = neg & 1;
    dpr = pe || (!ne && !pe) || (!p1 && ne && !n1);
    r1  = pe && (n1 || p1);
    return (pos + (~neg & BITS42) + dpr + r1) & BITS42;
}

// Division.
// Operands: register ACC and the argument 'val'.
// The result goes to ACC; the contents of RMR are undefined.
status_t besm6_divide(value_t val)
{
    alureg_t acc;
    alureg_t dividend, divisor;

    if (((val ^ (val << 1)) & BIT41) == 0) {
        // An unnormalized divisor: division by zero.
        return STOP_DIVZERO;
    }
    dividend = toalu(ACC);
    divisor  = toalu(val);

    acc.mantissa = au_divide(dividend.mantissa, divisor.mantissa);
    acc.exponent = dividend.exponent - divisor.exponent + 64;
    if (((acc.mantissa >> 1) ^ acc.mantissa) & BIT41) {
        // Normalization to the right
        acc.mantissa = acc.mantissa >> 1 | (acc.mantissa & BIT42);
        ++acc.exponent;
    }

    return normalize_and_round(acc, 0, 0);
}

// Multiplication.
// Operands: register ACC and the argument 'val'.
// The result goes to ACC and to bits 40-1 of RMR.
status_t besm6_multiply(value_t val)
{
    uint8_t neg = 0;
    alureg_t acc, word, a, b;
    uint64_t mr, alo, blo, ahi, bhi;

    uint64_t l;

    if (!ACC || !val) {
        // multiplication by zero is zero
        ACC = 0;
        RMR &= ~BITS40;
        return SCPE_OK;
    }
    acc  = toalu(ACC);
    word = toalu(val);

    a  = acc;
    b  = word;
    mr = 0;

    if (is_negative(&a)) {
        neg = 1;
        negate(&a);
    }
    if (is_negative(&b)) {
        neg ^= 1;
        negate(&b);
    }
    acc.exponent = a.exponent + b.exponent - 64;

    alo = a.mantissa & BITS(20);
    ahi = a.mantissa >> 20;

    blo = b.mantissa & BITS(20);
    bhi = b.mantissa >> 20;

    l = alo * blo + ((alo * bhi + ahi * blo) << 20);

    mr = l & BITS40;
    l >>= 40;

    acc.mantissa = l + ahi * bhi;

    if (neg && (acc.mantissa || mr)) {
        mr           = (~mr & BITS40) + 1;
        acc.mantissa = ((~acc.mantissa & BITS40) + (mr >> 40)) | BIT41 | BIT42;
        mr &= BITS40;
    }

    return normalize_and_round(acc, mr, mr != 0);
}

// Change the sign of the number in the accumulator ACC.
// The result goes to ACC; RMR is cleared.
status_t besm6_change_sign(int negate_acc)
{
    alureg_t acc;

    acc = toalu(ACC);
    if (negate_acc)
        negate(&acc);
    RMR = 0;
    return normalize_and_round(acc, 0, 0);
}

// Change the exponent of the number in the accumulator ACC.
// The result goes to ACC; RMR is cleared.
status_t besm6_add_exponent(int val)
{
    alureg_t acc;

    acc = toalu(ACC);
    acc.exponent += val;
    RMR = 0;
    return normalize_and_round(acc, 0, 0);
}

// Pack a value under a mask.
value_t besm6_pack(value_t val, value_t mask)
{
    value_t result;

    result = 0;
    for (; mask; mask >>= 1, val >>= 1)
        if (mask & 1) {
            result >>= 1;
            if (val & 1)
                result |= BIT48;
        }
    return result;
}

// Unpack a value under a mask.
value_t besm6_unpack(value_t val, value_t mask)
{
    value_t result;
    int i;

    result = 0;
    for (i = 0; i < 48; ++i) {
        result <<= 1;
        if (mask & BIT48) {
            if (val & BIT48)
                result |= 1;
            val <<= 1;
        }
        mask <<= 1;
    }
    return result;
}

// Count the one bits in a word.
int besm6_count_ones(value_t word)
{
    int c;

    for (c = 0; word; ++c)
        word &= word - 1;
    return c;
}

// Shift the accumulator ACC, shifting out into the low-order register RMR.
// The shift count is in the range -64..63.
void besm6_shift(int i)
{
    RMR = 0;
    if (i > 0) {
        // Shift right.
        if (i < 48) {
            RMR = (ACC << (48 - i)) & BITS48;
            ACC >>= i;
        } else {
            RMR = ACC >> (i - 48);
            ACC = 0;
        }
    } else if (i < 0) {
        // Shift left.
        i = -i;
        if (i < 48) {
            RMR = ACC >> (48 - i);
            ACC = (ACC << i) & BITS48;
        } else {
            RMR = (ACC << (i - 48)) & BITS48;
            ACC = 0;
        }
    }
}
