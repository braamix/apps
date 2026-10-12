// ============================================================================
// const_fold.c — constant folding.
//
// Constant folding evaluates expressions whose operands are all constants at
// compile time and replaces the instruction with a simpler one:
//
//   - A Unary or Binary with constant operand(s) becomes a Copy of the folded
//     result:   %0 = 6 / 2  →  %0 = 3 ;   %2 = a == a stays (a is a Var).
//   - A type-conversion instruction with a constant source becomes a Copy of a
//     new constant of the target kind:  SignExtend(ConstInt 3) → Copy(ConstLong 3).
//   - A conditional jump with a constant condition becomes an unconditional
//     Jump (always taken) or is deleted entirely (never taken) — which in turn
//     hands unreachable-code elimination fresh dead blocks to remove.
//
// This is the only pass that walks the flat Tac_Instruction list directly and
// needs no control-flow graph. Integer folding is done in 64-bit and the result
// is re-narrowed to the active target's value width for the operand kind (queried
// from target_config), so overflow wraps at the target's width — e.g. a BESM-6
// int is 41-bit, not the host's 32. Floating-point folding uses the host's
// float/double, and long double in software binary128 (as a double where the
// target's long double is one), so that no host's long double is relied on.
//
// See docs/TAC_Optimization.md §"Constant folding".
// ============================================================================

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "optimize.h"
#include "tac.h"
#include "target.h"
#include "xalloc.h"

// Forward declarations: fold_unary_const (defined first below) negates and
// complements the wide integer kinds through these helpers, which are defined
// further down.
static uint64_t const_to_uint64(const Tac_Const *c);
static Tac_Val *make_int_const_val(Tac_ConstKind kind, uint64_t bits);
static int target_signed_bits(Tac_ConstKind kind);

// Truthiness test: is this constant equal to zero? Used both to fold the logical
// NOT operator and to resolve conditional jumps. Covers all 11 scalar kinds.
// An integer constant as a jump table's index; false for any other.  A negative one
// is out of every table, as its unsigned value is.
static bool const_to_index(const Tac_Const *c, uint64_t *k)
{
    switch (c->kind) {
    case TAC_CONST_INT:
        *k = (uint32_t)c->u.int_val;
        return true;
    case TAC_CONST_UINT:
        *k = c->u.uint_val;
        return true;
    case TAC_CONST_LONG:
        *k = (uint64_t)c->u.long_val;
        return true;
    case TAC_CONST_ULONG:
        *k = c->u.ulong_val;
        return true;
    case TAC_CONST_LONG_LONG:
        *k = (uint64_t)c->u.long_long_val;
        return true;
    case TAC_CONST_ULONG_LONG:
        *k = c->u.ulong_long_val;
        return true;
    default:
        return false;
    }
}

static bool const_is_zero(const Tac_Const *c)
{
    switch (c->kind) {
    case TAC_CONST_INT:
        return c->u.int_val == 0;
    case TAC_CONST_LONG:
        return c->u.long_val == 0;
    case TAC_CONST_LONG_LONG:
        return c->u.long_long_val == 0;
    case TAC_CONST_UINT:
        return c->u.uint_val == 0;
    case TAC_CONST_ULONG:
        return c->u.ulong_val == 0;
    case TAC_CONST_ULONG_LONG:
        return c->u.ulong_long_val == 0;
    case TAC_CONST_FLOAT:
        return c->u.float_val == 0.0;
    case TAC_CONST_DOUBLE:
        return c->u.double_val == 0.0;
    case TAC_CONST_LONG_DOUBLE:
        return f128_is_zero(target_ld_round(c->u.long_double_val));
    case TAC_CONST_SCHAR:
        return c->u.char_val == 0;
    case TAC_CONST_UCHAR:
        return c->u.uchar_val == 0;
    }
    return false;
}

// Fold a unary operator applied to a constant. Returns a new constant-valued
// Tac_Val for the result, or NULL if the operation is not foldable.
//   - NOT yields an int 0/1 (the C logical-negation result type).
//   - NEGATE and COMPLEMENT preserve the source kind; the result is computed in
//     that kind so it wraps exactly as C would. Bitwise complement is undefined
//     for floating-point and so is rejected (NULL) for those kinds.
static Tac_Val *fold_unary_const(Tac_UnaryOperator op, const Tac_Const *src)
{
    Tac_Const *rc = NULL;

    switch (op) {
    case TAC_UNARY_NOT: {
        rc            = tac_new_const(TAC_CONST_INT);
        rc->u.int_val = const_is_zero(src) ? 1 : 0;
        break;
    }
    case TAC_UNARY_NEGATE:
    case TAC_UNARY_NEGATE_UNSIGNED:
    case TAC_UNARY_NEGATE_DOUBLE: {
        switch (src->kind) {
        case TAC_CONST_INT:
        case TAC_CONST_LONG:
        case TAC_CONST_LONG_LONG:
        case TAC_CONST_UINT:
        case TAC_CONST_ULONG:
        case TAC_CONST_ULONG_LONG:
            // Negate in 64-bit two's complement, then wrap to the target's value
            // width for the kind (so e.g. a BESM-6 41-bit result is not truncated
            // at the host int's 32 bits).
            return make_int_const_val(src->kind, (uint64_t)0 - const_to_uint64(src));
        case TAC_CONST_FLOAT:
            rc              = tac_new_const(src->kind);
            rc->u.float_val = -src->u.float_val;
            break;
        case TAC_CONST_DOUBLE:
            rc               = tac_new_const(src->kind);
            rc->u.double_val = -src->u.double_val;
            break;
        case TAC_CONST_LONG_DOUBLE:
            rc                    = tac_new_const(src->kind);
            rc->u.long_double_val = f128_neg(target_ld_round(src->u.long_double_val));
            break;
        case TAC_CONST_SCHAR:
            rc             = tac_new_const(src->kind);
            rc->u.char_val = -src->u.char_val;
            break;
        case TAC_CONST_UCHAR:
            rc              = tac_new_const(src->kind);
            rc->u.uchar_val = (unsigned char)(-src->u.uchar_val);
            break;
        }
        break;
    }
    case TAC_UNARY_COMPLEMENT:
    case TAC_UNARY_COMPLEMENT_UNSIGNED: {
        // Folded in the constant's own (signed or unsigned) type, so a single
        // body handles both the signed and unsigned complement operators.
        switch (src->kind) {
        case TAC_CONST_INT:
        case TAC_CONST_LONG:
        case TAC_CONST_LONG_LONG:
        case TAC_CONST_UINT:
        case TAC_CONST_ULONG:
        case TAC_CONST_ULONG_LONG:
            // Complement in 64-bit, then wrap to the target's value width.
            return make_int_const_val(src->kind, ~const_to_uint64(src));
        case TAC_CONST_SCHAR:
            rc             = tac_new_const(src->kind);
            rc->u.char_val = (int)(signed char)(~src->u.char_val);
            break;
        case TAC_CONST_UCHAR:
            rc              = tac_new_const(src->kind);
            rc->u.uchar_val = (unsigned char)(~src->u.uchar_val);
            break;
        default:
            // Floats: complement is undefined in C; should not appear in well-typed TAC.
            return NULL;
        }
        break;
    }
    case TAC_UNARY_SQRT_DOUBLE:
        // The host's sqrt is IEEE, correctly rounded as the targets' instruction is.  A
        // negative operand or a NaN is left alone: the NaN the target makes has a sign
        // of its own (negative on x86-64, positive on RISC-V and ARM).
        if (src->kind != TAC_CONST_DOUBLE || src->u.double_val < 0 || isnan(src->u.double_val))
            return NULL;
        rc               = tac_new_const(TAC_CONST_DOUBLE);
        rc->u.double_val = target_double_round(sqrt(src->u.double_val));
        break;
    }

    Tac_Val *rv    = tac_new_val(TAC_VAL_CONSTANT);
    rv->u.constant = rc;
    return rv;
}

// True for the eight integer constant kinds (everything except the three
// floating-point kinds). Integer and float folding follow different rules.
static bool const_is_integer_kind(Tac_ConstKind k)
{
    return k == TAC_CONST_INT || k == TAC_CONST_LONG || k == TAC_CONST_LONG_LONG ||
           k == TAC_CONST_UINT || k == TAC_CONST_ULONG || k == TAC_CONST_ULONG_LONG ||
           k == TAC_CONST_SCHAR || k == TAC_CONST_UCHAR;
}

// Widen any integer constant to a signed 64-bit value (sign-extending the
// signed kinds, zero-extending the unsigned ones). Signed binary operators are
// evaluated through this view.
//
// A signed constant reads as the code generator emits it: narrowed to the target's
// signed width.  A literal too wide for it (2^48 - 659 as a BESM-6 long long, whose
// value bits are 41) is stored unmasked, but means its low bits there.
//
// An unsigned int, long or long long constant reads the same way, as the signed type of
// its width: a signed operator finds one only where a cast between the two types (which
// emits no TAC) was copy-propagated through, so `int i = (int)3000000000u` leaves the
// unsigned constant in `i >= 0`, which must compare a negative int.
static int64_t const_to_int64(const Tac_Const *c)
{
    switch (c->kind) {
    case TAC_CONST_INT:
        return sign_narrow((uint64_t)c->u.int_val, target_signed_bits(TAC_CONST_INT));
    case TAC_CONST_LONG:
        return sign_narrow((uint64_t)c->u.long_val, target_signed_bits(TAC_CONST_LONG));
    case TAC_CONST_LONG_LONG:
        return sign_narrow((uint64_t)c->u.long_long_val, target_signed_bits(TAC_CONST_LONG_LONG));
    case TAC_CONST_UINT:
        return sign_narrow((uint64_t)c->u.uint_val, target_signed_bits(TAC_CONST_INT));
    case TAC_CONST_ULONG:
        return sign_narrow((uint64_t)c->u.ulong_val, target_signed_bits(TAC_CONST_LONG));
    case TAC_CONST_ULONG_LONG:
        return sign_narrow((uint64_t)c->u.ulong_long_val, target_signed_bits(TAC_CONST_LONG_LONG));
    case TAC_CONST_SCHAR:
        return c->u.char_val;
    case TAC_CONST_UCHAR:
        return c->u.uchar_val;
    default:
        return 0;
    }
}

// Widen any integer constant to a 64-bit bit pattern. Unsigned operators and
// the wrapping arithmetic operators (add/sub/mul, bitwise, shifts) are evaluated
// through this view; the result is re-narrowed by make_int_const_val.
//
// A *negative* signed constant is reduced to the *unsigned view of its target
// word* — its two's-complement pattern zero-extended from the target signed width
// (e.g. 41 bits on BESM-6).  This matters because BESM-6 stores a signed `int` in a
// word whose upper bits are zero, so reinterpreting a negative value as unsigned
// yields the 41-bit pattern, not a 64-bit sign-extension: e.g. (unsigned)(-1) is
// 2^41-1, not 2^64-1.  Unsigned divide/remainder/compare and logical right shift —
// which depend on the high bits — then fold to the value the hardware computes.
// A positive literal too wide for the signed width is reduced the same way, to the
// low bits the code generator emits for it.  With no target configured the width is
// 0 and unsigned_narrow is a no-op, so the host behavior is unchanged.
static uint64_t const_to_uint64(const Tac_Const *c)
{
    switch (c->kind) {
    case TAC_CONST_INT:
        return unsigned_narrow((uint64_t)(int64_t)c->u.int_val, target_signed_bits(TAC_CONST_INT));
    case TAC_CONST_LONG:
        return unsigned_narrow((uint64_t)c->u.long_val, target_signed_bits(TAC_CONST_LONG));
    case TAC_CONST_LONG_LONG:
        return unsigned_narrow((uint64_t)c->u.long_long_val,
                               target_signed_bits(TAC_CONST_LONG_LONG));
    case TAC_CONST_UINT:
        return c->u.uint_val;
    case TAC_CONST_ULONG:
        return c->u.ulong_val;
    case TAC_CONST_ULONG_LONG:
        return c->u.ulong_long_val;
    case TAC_CONST_SCHAR:
        return (uint64_t)(int64_t)(int8_t)c->u.char_val;
    case TAC_CONST_UCHAR:
        return c->u.uchar_val;
    default:
        return 0;
    }
}

// Storage width in bits of the operand a shift acts on, ON THE ACTIVE TARGET: the
// point past which a shift count is out of range.  This is the *storage* width,
// size*8, not the signed value width -- a BESM-6 `int` holds 41 bits of value in a
// 48-bit word, and its shift unit shifts the whole word.
//
// THIS USED TO BE A MASK OF 31 OR 63, the host's widths, and folding `count & 31`
// silently turned every in-range BESM-6 shift past 31 into a different one:
// `1u << 36` folded to `1u << (36 & 31)` == 020, and the same expression evaluated
// at run time -- a variable count reaches the shift unit unmasked -- gave 2^36.  A
// constant folder that disagrees with the code generator is worse than no folder,
// and this disagreed only for counts a 32-bit target could not have.
//
// C leaves a count of the promoted operand's width or more undefined, so an
// out-of-range count may fold to anything; 0 is chosen because it is what the
// BESM-6 shift unit produces (a shift of 48 or more clears the accumulator) and
// what an x86 shift of a value already reduced to nothing produces.  What matters
// is that an IN-RANGE count is exact, which masking made it not.
//
// Character operands have no shift of their own: C integer promotions widen them
// to `int` before the shift, so a char shift is an int shift and uses the int
// width, not the 8-bit char width.  Using 8 here would mis-fold e.g.
// `(unsigned char)250 >> 31`, an in-range shift of a promoted int, to 0 by the
// out-of-range rule below, where the shift the hardware performs gives 0 anyway --
// but on BESM-6, where an int is 48 bits, `(unsigned char)250 << 40` is in range
// and must not be thrown away.
static int const_shift_bits(Tac_ConstKind k)
{
    switch (k) {
    case TAC_CONST_SCHAR:
    case TAC_CONST_UCHAR:
    case TAC_CONST_INT:
    case TAC_CONST_UINT:
        return target_config ? (int)target_config->int_size * 8 : 32;
    case TAC_CONST_LONG:
    case TAC_CONST_ULONG:
        return target_config ? (int)target_config->long_size * 8 : 64;
    default:
        return target_config ? (int)target_config->llong_size * 8 : 64;
    }
}

// The shift count to fold with, or -1 when C says the shift is undefined and the
// folder answers 0.  A negative count is undefined too; u2 is the unsigned view, so
// one arrives here as a huge value and is rejected by the same test.
static int const_shift_count(Tac_ConstKind k, uint64_t u2)
{
    return u2 < (uint64_t)const_shift_bits(k) ? (int)u2 : -1;
}

// Signed value width (in bits) of a signed integer constant kind on the active
// target.  Narrowing a folded result uses this so overflow wraps at the target's
// width, not the host C type's: e.g. BESM-6 int/long/long long are 41-bit, where
// the host `int` is 32-bit.  Returns 0 if no target is configured (caller then
// keeps the host C narrowing).  `target_config` defaults to x86_64, whose widths
// match the LP64 host, so the machine-independent optimizer tests are unaffected.
static int target_signed_bits(Tac_ConstKind kind)
{
    if (!target_config)
        return 0;
    switch (kind) {
    case TAC_CONST_INT:
        return target_config->int_bits;
    case TAC_CONST_LONG:
        return target_config->long_bits;
    case TAC_CONST_LONG_LONG:
        return target_config->llong_bits;
    default:
        // No TAC short constant kind exists; reference short_bits here so the
        // Target field stays "used" for static analysis.
        return target_config->short_bits;
    }
}

// Plain `char` is signed or unsigned per the active target (default signed when no
// target is configured).  Used to pick the result kind when folding a truncation to
// char so a plain-`char` constant on an unsigned-`char` target becomes TAC_CONST_UCHAR
// and folds through the existing unsigned-char paths.
static bool char_const_signed(void)
{
    return !target_config || target_config->char_signed;
}

// Unsigned value width (in bits) of an unsigned integer constant kind on the
// active target: always the full storage width, size*8 (BESM-6 unsigned ints use
// all 48 bits of the word).  Returns 0 if no target is configured.
static int target_unsigned_bits(Tac_ConstKind kind)
{
    if (!target_config)
        return 0;
    switch (kind) {
    case TAC_CONST_UINT:
        return (int)target_config->int_size * 8;
    case TAC_CONST_ULONG:
        return (int)target_config->long_size * 8;
    case TAC_CONST_ULONG_LONG:
        return (int)target_config->llong_size * 8;
    default:
        return 0;
    }
}

// The bits of a signed value sign-extended to `dst_kind`. To an unsigned word kind
// the conversion keeps the word, which holds the value at the signed width of the
// matching signed kind: a negative value wraps there, not at the unsigned width.
// On BESM-6, where signed values are 41 bits of the 48-bit word,
// (unsigned)(signed char)-10 is 2^41 - 10; elsewhere the two widths agree.
static uint64_t sign_extend_bits(int64_t v, int dst_kind)
{
    Tac_ConstKind signed_kind;
    switch (dst_kind) {
    case TAC_CONST_UINT:
        signed_kind = TAC_CONST_INT;
        break;
    case TAC_CONST_ULONG:
        signed_kind = TAC_CONST_LONG;
        break;
    case TAC_CONST_ULONG_LONG:
        signed_kind = TAC_CONST_LONG_LONG;
        break;
    default:
        return (uint64_t)v;
    }
    return v < 0 ? unsigned_narrow((uint64_t)v, target_signed_bits(signed_kind)) : (uint64_t)v;
}

// Build a constant-valued Tac_Val of `kind` from a 64-bit result `bits`, wrapping
// to the active target's value width for that kind.  This is where overflow
// wrapping happens: signed kinds sign-extend from the target signed width and
// unsigned kinds mask to the target storage width (size*8).  When the width is
// unavailable (no target) the previous host C narrowing is used.
static Tac_Val *make_int_const_val(Tac_ConstKind kind, uint64_t bits)
{
    Tac_Const *rc = tac_new_const(kind);
    switch (kind) {
    case TAC_CONST_INT:
        rc->u.int_val = sign_narrow(bits, target_signed_bits(kind));
        break;
    case TAC_CONST_LONG:
        rc->u.long_val = (long)sign_narrow(bits, target_signed_bits(kind));
        break;
    case TAC_CONST_LONG_LONG:
        rc->u.long_long_val = (long long)sign_narrow(bits, target_signed_bits(kind));
        break;
    case TAC_CONST_UINT:
        rc->u.uint_val = unsigned_narrow(bits, target_unsigned_bits(kind));
        break;
    case TAC_CONST_ULONG:
        rc->u.ulong_val = (unsigned long)unsigned_narrow(bits, target_unsigned_bits(kind));
        break;
    case TAC_CONST_ULONG_LONG:
        rc->u.ulong_long_val =
            (unsigned long long)unsigned_narrow(bits, target_unsigned_bits(kind));
        break;
    case TAC_CONST_SCHAR:
        rc->u.char_val = (int)(int8_t)bits;
        break;
    case TAC_CONST_UCHAR:
        rc->u.uchar_val = (unsigned char)bits;
        break;
    default:
        break;
    }
    Tac_Val *rv    = tac_new_val(TAC_VAL_CONSTANT);
    rv->u.constant = rc;
    return rv;
}

// A float result, rounded to float precision where float is narrower than double, as
// the target computes it (FLT_EVAL_METHOD 0), or where double is single too (AVR).
// BESM-6 float is the double format.
static double round_float(double d)
{
    if (!target_config || target_config->float_size < target_config->double_size ||
        target_double_is_single())
        // cppcheck-suppress suspiciousFloatingPointCast ; rounding to float is the point
        return (float)d;
    return d;
}

// A long double constant's value as the target holds it: an x87 long double has a
// 64-bit significand, so a constant read from the source in binary128 is rounded first.
static Float128 ld_value(const Tac_Const *c)
{
    return target_ld_round(c->u.long_double_val);
}

// Whether the target's long double is a double (BESM-6): then it folds as one.
static bool ld_is_double(void)
{
    return target_config && target_config->ldouble_size <= 8;
}

static Float128 ld_from_int(int64_t v, bool is_unsigned)
{
    if (ld_is_double())
        return f128_from_double(is_unsigned ? target_double_from_u64((uint64_t)v)
                                            : target_double_from_i64(v));
    return is_unsigned ? f128_from_u64((uint64_t)v) : f128_from_i64(v);
}

// A long double operation on binary128 values, as the target computes it.
static Float128 ld_arith(Tac_BinaryOperator op, Float128 a, Float128 b)
{
    if (ld_is_double()) {
        double x = f128_to_double(a), y = f128_to_double(b);
        switch (op) {
        case TAC_BINARY_ADD:
        case TAC_BINARY_ADD_DOUBLE:
            return f128_from_double(x + y);
        case TAC_BINARY_SUBTRACT:
        case TAC_BINARY_SUBTRACT_DOUBLE:
            return f128_from_double(x - y);
        case TAC_BINARY_MULTIPLY:
        case TAC_BINARY_MULTIPLY_DOUBLE:
            return f128_from_double(x * y);
        default:
            return f128_from_double(x / y);
        }
    }
    switch (op) {
    case TAC_BINARY_ADD:
    case TAC_BINARY_ADD_DOUBLE:
        return f128_add(a, b);
    case TAC_BINARY_SUBTRACT:
    case TAC_BINARY_SUBTRACT_DOUBLE:
        return f128_sub(a, b);
    case TAC_BINARY_MULTIPLY:
    case TAC_BINARY_MULTIPLY_DOUBLE:
        return f128_mul(a, b);
    default:
        return f128_div(a, b);
    }
}

// Fold a binary operator on two floating-point constants of the *same* kind.
// Arithmetic operators produce a constant of that kind; the relational and
// equality operators produce an int 0/1 (the C comparison result type). Returns
// NULL when the kinds differ or the operator is not a float operator.
static Tac_Val *fold_binary_float(Tac_BinaryOperator op, const Tac_Const *c1, const Tac_Const *c2)
{
    if (c1->kind != c2->kind)
        return NULL;

    if (c1->kind == TAC_CONST_LONG_DOUBLE) {
        Float128 a = ld_value(c1), b = ld_value(c2);
        int c = f128_cmp(a, b); // 2 when unordered
        switch (op) {
        case TAC_BINARY_ADD:
        case TAC_BINARY_ADD_DOUBLE:
        case TAC_BINARY_SUBTRACT:
        case TAC_BINARY_SUBTRACT_DOUBLE:
        case TAC_BINARY_MULTIPLY:
        case TAC_BINARY_MULTIPLY_DOUBLE:
        case TAC_BINARY_DIVIDE:
        case TAC_BINARY_DIVIDE_DOUBLE:
            break;
        case TAC_BINARY_EQUAL:
            return make_int_const_val(TAC_CONST_INT, c == 0);
        case TAC_BINARY_NOT_EQUAL:
            return make_int_const_val(TAC_CONST_INT, c != 0);
        case TAC_BINARY_LESS_THAN:
        case TAC_BINARY_LESS_THAN_DOUBLE:
            return make_int_const_val(TAC_CONST_INT, c == -1);
        case TAC_BINARY_LESS_OR_EQUAL:
        case TAC_BINARY_LESS_OR_EQUAL_DOUBLE:
            return make_int_const_val(TAC_CONST_INT, c == -1 || c == 0);
        case TAC_BINARY_GREATER_THAN:
        case TAC_BINARY_GREATER_THAN_DOUBLE:
            return make_int_const_val(TAC_CONST_INT, c == 1);
        case TAC_BINARY_GREATER_OR_EQUAL:
        case TAC_BINARY_GREATER_OR_EQUAL_DOUBLE:
            return make_int_const_val(TAC_CONST_INT, c == 1 || c == 0);
        default:
            return NULL;
        }
        Tac_Const *rc         = tac_new_const(TAC_CONST_LONG_DOUBLE);
        rc->u.long_double_val = target_ld_round(ld_arith(op, a, b));
        Tac_Val *rv           = tac_new_val(TAC_VAL_CONSTANT);
        rv->u.constant        = rc;
        return rv;
    }

    if (c1->kind != TAC_CONST_FLOAT && c1->kind != TAC_CONST_DOUBLE)
        return NULL;

    double dv1 = (c1->kind == TAC_CONST_FLOAT) ? c1->u.float_val : c1->u.double_val;
    double dv2 = (c2->kind == TAC_CONST_FLOAT) ? c2->u.float_val : c2->u.double_val;
    double dr;
    switch (op) {
    case TAC_BINARY_ADD:
    case TAC_BINARY_ADD_DOUBLE:
        dr = dv1 + dv2;
        break;
    case TAC_BINARY_SUBTRACT:
    case TAC_BINARY_SUBTRACT_DOUBLE:
        dr = dv1 - dv2;
        break;
    case TAC_BINARY_MULTIPLY:
    case TAC_BINARY_MULTIPLY_DOUBLE:
        dr = dv1 * dv2;
        break;
    case TAC_BINARY_DIVIDE:
    case TAC_BINARY_DIVIDE_DOUBLE:
        dr = dv1 / dv2;
        break;
    case TAC_BINARY_EQUAL:
        return make_int_const_val(TAC_CONST_INT, dv1 == dv2);
    case TAC_BINARY_NOT_EQUAL:
        return make_int_const_val(TAC_CONST_INT, dv1 != dv2);
    case TAC_BINARY_LESS_THAN:
    case TAC_BINARY_LESS_THAN_DOUBLE:
        return make_int_const_val(TAC_CONST_INT, dv1 < dv2);
    case TAC_BINARY_LESS_OR_EQUAL:
    case TAC_BINARY_LESS_OR_EQUAL_DOUBLE:
        return make_int_const_val(TAC_CONST_INT, dv1 <= dv2);
    case TAC_BINARY_GREATER_THAN:
    case TAC_BINARY_GREATER_THAN_DOUBLE:
        return make_int_const_val(TAC_CONST_INT, dv1 > dv2);
    case TAC_BINARY_GREATER_OR_EQUAL:
    case TAC_BINARY_GREATER_OR_EQUAL_DOUBLE:
        return make_int_const_val(TAC_CONST_INT, dv1 >= dv2);
    default:
        return NULL;
    }
    Tac_Const *rc = tac_new_const(c1->kind);
    if (c1->kind == TAC_CONST_FLOAT)
        rc->u.float_val = round_float(dr);
    else
        rc->u.double_val = target_double_round(dr);
    Tac_Val *rv    = tac_new_val(TAC_VAL_CONSTANT);
    rv->u.constant = rc;
    return rv;
}

// Map an integer constant kind to its unsigned counterpart of the same width.
// The result of an unsigned operator is unsigned regardless of how its operands
// happen to be typed: a literal `1` (the step of `u--`) is an int-kind constant,
// so narrowing the wrapped result to that signed kind would sign-narrow a 48-bit
// unsigned value to the 41-bit signed width.  Forcing the unsigned kind keeps the
// full storage width.
static Tac_ConstKind const_kind_to_unsigned(Tac_ConstKind k)
{
    switch (k) {
    case TAC_CONST_INT:
        return TAC_CONST_UINT;
    case TAC_CONST_LONG:
        return TAC_CONST_ULONG;
    case TAC_CONST_LONG_LONG:
        return TAC_CONST_ULONG_LONG;
    case TAC_CONST_SCHAR:
        return TAC_CONST_UCHAR;
    default:
        return k; // already unsigned (or non-integer)
    }
}

// True for the unsigned-variant arithmetic/shift operators, whose result is an
// unsigned value (and so must be narrowed to the unsigned storage width).
static bool binop_is_unsigned(Tac_BinaryOperator op)
{
    switch (op) {
    case TAC_BINARY_ADD_UNSIGNED:
    case TAC_BINARY_SUBTRACT_UNSIGNED:
    case TAC_BINARY_MULTIPLY_UNSIGNED:
    case TAC_BINARY_DIVIDE_UNSIGNED:
    case TAC_BINARY_REMAINDER_UNSIGNED:
    case TAC_BINARY_RIGHT_SHIFT_LOGICAL:
        return true;
    default:
        return false;
    }
}

// Fold a binary operator on two constant operands. Returns a new constant-valued
// Tac_Val, or NULL if not foldable. If either operand is floating-point the work
// is delegated to fold_binary_float; the rest of this function handles integers.
//
// Division and remainder by a zero constant are *not* folded (returns NULL): we
// leave the instruction in place rather than invoke undefined behavior at
// compile time. Comparisons produce an int 0/1; the signed/unsigned operator
// variants pick the signed (s1/s2) or unsigned (u1/u2) 64-bit view accordingly.
// Wrapping arithmetic is done in uint64 and narrowed back to c1's kind — forced
// to the unsigned counterpart for the unsigned-variant operators.
// `dst_kind` is the constant kind of the destination's type, or -1 when unknown.
static Tac_Val *fold_binary_const(Tac_BinaryOperator op, const Tac_Const *c1, const Tac_Const *c2,
                                  int dst_kind)
{
    if (!const_is_integer_kind(c1->kind) || !const_is_integer_kind(c2->kind))
        return fold_binary_float(op, c1, c2);

    if ((op == TAC_BINARY_DIVIDE || op == TAC_BINARY_REMAINDER ||
         op == TAC_BINARY_DIVIDE_UNSIGNED || op == TAC_BINARY_REMAINDER_UNSIGNED) &&
        const_is_zero(c2))
        return NULL;

    // Both a signed and an unsigned 64-bit view of each operand; operators pick
    // whichever matches their C semantics.
    int64_t s1 = const_to_int64(c1), s2 = const_to_int64(c2);
    uint64_t u1 = const_to_uint64(c1), u2 = const_to_uint64(c2);
    uint64_t result;

    switch (op) {
    case TAC_BINARY_ADD:
    case TAC_BINARY_ADD_UNSIGNED:
        result = u1 + u2;
        break;
    case TAC_BINARY_SUBTRACT:
    case TAC_BINARY_SUBTRACT_UNSIGNED:
        result = u1 - u2;
        break;
    case TAC_BINARY_MULTIPLY:
    case TAC_BINARY_MULTIPLY_UNSIGNED:
        result = u1 * u2;
        break;
    case TAC_BINARY_DIVIDE:
        result = (uint64_t)(s1 / s2);
        break;
    case TAC_BINARY_REMAINDER:
        result = (uint64_t)(s1 % s2);
        break;
    case TAC_BINARY_DIVIDE_UNSIGNED:
        result = u1 / u2;
        break;
    case TAC_BINARY_REMAINDER_UNSIGNED:
        result = u1 % u2;
        break;

    case TAC_BINARY_EQUAL:
        return make_int_const_val(TAC_CONST_INT, u1 == u2);
    case TAC_BINARY_NOT_EQUAL:
        return make_int_const_val(TAC_CONST_INT, u1 != u2);
    case TAC_BINARY_LESS_THAN:
        return make_int_const_val(TAC_CONST_INT, s1 < s2);
    case TAC_BINARY_LESS_OR_EQUAL:
        return make_int_const_val(TAC_CONST_INT, s1 <= s2);
    case TAC_BINARY_GREATER_THAN:
        return make_int_const_val(TAC_CONST_INT, s1 > s2);
    case TAC_BINARY_GREATER_OR_EQUAL:
        return make_int_const_val(TAC_CONST_INT, s1 >= s2);

    case TAC_BINARY_LESS_THAN_UNSIGNED:
        return make_int_const_val(TAC_CONST_INT, u1 < u2);
    case TAC_BINARY_LESS_OR_EQUAL_UNSIGNED:
        return make_int_const_val(TAC_CONST_INT, u1 <= u2);
    case TAC_BINARY_GREATER_THAN_UNSIGNED:
        return make_int_const_val(TAC_CONST_INT, u1 > u2);
    case TAC_BINARY_GREATER_OR_EQUAL_UNSIGNED:
        return make_int_const_val(TAC_CONST_INT, u1 >= u2);

    case TAC_BINARY_BITWISE_AND:
        result = u1 & u2;
        break;
    case TAC_BINARY_BITWISE_OR:
        result = u1 | u2;
        break;
    case TAC_BINARY_BITWISE_XOR:
        result = u1 ^ u2;
        break;

    case TAC_BINARY_LEFT_SHIFT: {
        int amt = const_shift_count(c1->kind, u2);
        result  = amt < 0 ? 0 : u1 << (unsigned)amt;
        break;
    }
    case TAC_BINARY_RIGHT_SHIFT: {
        int amt = const_shift_count(c1->kind, u2);
        if (amt < 0) {
            result = 0;
        } else if (target_config && target_config->right_shift_is_logical) {
            // Target (BESM-6) shifts right logically even for signed operands: zero-fill
            // the operand's target-width bit pattern, matching the backend's shift unit.
            // u1 is sign-extended to 64 bits, so mask it back to the signed value width
            // first (e.g. 41 bits on BESM-6) before the logical shift.
            int w        = target_signed_bits(c1->kind);
            uint64_t pat = unsigned_narrow(u1, w);
            result       = pat >> (unsigned)amt;
        } else {
            // Arithmetic right shift: operate on the signed view so the sign bit
            // is replicated (sign-preserving), matching signed >> in C.
            result = (uint64_t)(s1 >> amt);
        }
        break;
    }
    case TAC_BINARY_RIGHT_SHIFT_LOGICAL: {
        // Logical right shift: operate on the unsigned view (zero-fill).
        int amt = const_shift_count(c1->kind, u2);
        result  = amt < 0 ? 0 : u1 >> (unsigned)amt;
        break;
    }
    default:
        return NULL;
    }

    // The result has the destination's type: the first operand's constant can have
    // another, through a same-width cast that emitted nothing (`unsigned u = 5; u << 45`
    // has an int 5, and its result would be narrowed to a BESM-6 int's 41 bits).
    Tac_ConstKind result_kind = dst_kind >= 0           ? (Tac_ConstKind)dst_kind
                                : binop_is_unsigned(op) ? const_kind_to_unsigned(c1->kind)
                                                        : c1->kind;
    return make_int_const_val(result_kind, result);
}

// The constant kind of the int, long or long long variable `v` of function `fn`, or -1.
static int var_const_kind(const Tac_TopLevel *fn, const Tac_Val *v)
{
    if (!fn || !v || v->kind != TAC_VAL_VAR)
        return -1;
    for (int list = 0; list < 2; list++) {
        for (const Tac_Param *p = list ? fn->u.function.locals : fn->u.function.params; p;
             p                  = p->next) {
            if (!p->type || strcmp(p->name, v->u.var_name) != 0)
                continue;
            switch (p->type->kind) {
            case TAC_TYPE_INT:
                return TAC_CONST_INT;
            case TAC_TYPE_UINT:
                return TAC_CONST_UINT;
            case TAC_TYPE_LONG:
                return TAC_CONST_LONG;
            case TAC_TYPE_ULONG:
                return TAC_CONST_ULONG;
            case TAC_TYPE_LONG_LONG:
                return TAC_CONST_LONG_LONG;
            case TAC_TYPE_ULONG_LONG:
                return TAC_CONST_ULONG_LONG;
            default:
                return -1;
            }
        }
    }
    return -1;
}

// True for all 14 type-conversion instruction kinds (the three integer-width
// conversions plus the twelve floating-point conversions). They all share the
// same {src, dst} union layout, so the driver can treat them uniformly.
// The comparison `b op' a` equal to `a op b`, or -1 for an operator that is not a
// comparison.
static Tac_BinaryOperator mirror_comparison(Tac_BinaryOperator op)
{
    switch (op) {
    case TAC_BINARY_EQUAL:
    case TAC_BINARY_NOT_EQUAL:
        return op;
    case TAC_BINARY_LESS_THAN:
        return TAC_BINARY_GREATER_THAN;
    case TAC_BINARY_GREATER_THAN:
        return TAC_BINARY_LESS_THAN;
    case TAC_BINARY_LESS_OR_EQUAL:
        return TAC_BINARY_GREATER_OR_EQUAL;
    case TAC_BINARY_GREATER_OR_EQUAL:
        return TAC_BINARY_LESS_OR_EQUAL;
    case TAC_BINARY_LESS_THAN_UNSIGNED:
        return TAC_BINARY_GREATER_THAN_UNSIGNED;
    case TAC_BINARY_GREATER_THAN_UNSIGNED:
        return TAC_BINARY_LESS_THAN_UNSIGNED;
    case TAC_BINARY_LESS_OR_EQUAL_UNSIGNED:
        return TAC_BINARY_GREATER_OR_EQUAL_UNSIGNED;
    case TAC_BINARY_GREATER_OR_EQUAL_UNSIGNED:
        return TAC_BINARY_LESS_OR_EQUAL_UNSIGNED;
    case TAC_BINARY_LESS_THAN_DOUBLE:
        return TAC_BINARY_GREATER_THAN_DOUBLE;
    case TAC_BINARY_GREATER_THAN_DOUBLE:
        return TAC_BINARY_LESS_THAN_DOUBLE;
    case TAC_BINARY_LESS_OR_EQUAL_DOUBLE:
        return TAC_BINARY_GREATER_OR_EQUAL_DOUBLE;
    case TAC_BINARY_GREATER_OR_EQUAL_DOUBLE:
        return TAC_BINARY_LESS_OR_EQUAL_DOUBLE;
    default:
        return (Tac_BinaryOperator)-1;
    }
}

static bool is_conversion(Tac_InstructionKind k)
{
    switch (k) {
    case TAC_INSTRUCTION_SIGN_EXTEND:
    case TAC_INSTRUCTION_ZERO_EXTEND:
    case TAC_INSTRUCTION_TRUNCATE:
    case TAC_INSTRUCTION_INT_TO_DOUBLE:
    case TAC_INSTRUCTION_UINT_TO_DOUBLE:
    case TAC_INSTRUCTION_DOUBLE_TO_INT:
    case TAC_INSTRUCTION_DOUBLE_TO_UINT:
    case TAC_INSTRUCTION_INT_TO_FLOAT:
    case TAC_INSTRUCTION_UINT_TO_FLOAT:
    case TAC_INSTRUCTION_FLOAT_TO_INT:
    case TAC_INSTRUCTION_FLOAT_TO_UINT:
    case TAC_INSTRUCTION_FLOAT_TO_DOUBLE:
    case TAC_INSTRUCTION_DOUBLE_TO_FLOAT:
    case TAC_INSTRUCTION_INT_TO_LONG_DOUBLE:
    case TAC_INSTRUCTION_UINT_TO_LONG_DOUBLE:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_INT:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_UINT:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_DOUBLE:
    case TAC_INSTRUCTION_DOUBLE_TO_LONG_DOUBLE:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_FLOAT:
    case TAC_INSTRUCTION_FLOAT_TO_LONG_DOUBLE:
        return true;
    default:
        return false;
    }
}

// Fold a type conversion of a constant source into a new constant of the target
// kind. Returns NULL if the source kind does not match what the conversion
// expects (e.g. a Truncate applied to a float). The target kind is implied by
// the conversion instruction: SignExtend widens char→int→long→long long;
// ZeroExtend does the unsigned ladder; Truncate narrows; the *To* conversions
// move between integer and floating-point. Float→int conversions truncate
// toward zero, as C casts do.
// `dst_kind` is the destination's Tac_ConstKind for the three integer-width
// conversions (SignExtend/ZeroExtend/Truncate), or -1 when the lowering did not supply
// one.  When known, the folded result is labelled with it — this is what lets a
// promotion `unsigned char → int` fold to a signed constant while an explicit cast
// `unsigned char → unsigned int` folds to an unsigned one, even though both lower to the
// same ZERO_EXTEND.  When -1, the legacy source-derived kind selection is kept.
static Tac_Val *fold_conversion(Tac_InstructionKind kind, const Tac_Const *src, int dst_kind)
{
    Tac_Const *rc = NULL;

    if (dst_kind == TAC_DST_KIND_NO_FOLD)
        return NULL;
    switch (kind) {
        /* ---- integer width conversions ---- */

    case TAC_INSTRUCTION_SIGN_EXTEND:
        if (dst_kind >= 0 && const_is_integer_kind(src->kind))
            return make_int_const_val((Tac_ConstKind)dst_kind,
                                      sign_extend_bits(const_to_int64(src), dst_kind));
        rc = tac_new_const(src->kind == TAC_CONST_SCHAR  ? TAC_CONST_INT
                           : src->kind == TAC_CONST_INT  ? TAC_CONST_LONG
                           : src->kind == TAC_CONST_LONG ? TAC_CONST_LONG_LONG
                                                         : TAC_CONST_INT);
        switch (src->kind) {
        case TAC_CONST_SCHAR:
            rc->u.int_val = (int)(int8_t)src->u.char_val;
            break;
        case TAC_CONST_INT:
            rc->u.long_val = (long)src->u.int_val;
            break;
        case TAC_CONST_LONG:
            rc->u.long_long_val = (long long)src->u.long_val;
            break;
        default:
            tac_free_const(rc);
            return NULL;
        }
        break;

    case TAC_INSTRUCTION_ZERO_EXTEND:
        // The source's own bits: copy propagation may hand an unsigned char view
        // the signed char constant it was copied from, and -1 must widen to 255.
        if (dst_kind >= 0 && const_is_integer_kind(src->kind))
            return make_int_const_val((Tac_ConstKind)dst_kind,
                                      src->kind == TAC_CONST_SCHAR
                                          ? (uint64_t)(uint8_t)src->u.char_val
                                          : const_to_uint64(src));
        rc = tac_new_const(src->kind == TAC_CONST_UCHAR   ? TAC_CONST_UINT
                           : src->kind == TAC_CONST_UINT  ? TAC_CONST_ULONG
                           : src->kind == TAC_CONST_ULONG ? TAC_CONST_ULONG_LONG
                                                          : TAC_CONST_UINT);
        switch (src->kind) {
        case TAC_CONST_UCHAR:
            rc->u.uint_val = (unsigned)src->u.uchar_val;
            break;
        case TAC_CONST_UINT:
            rc->u.ulong_val = (unsigned long)src->u.uint_val;
            break;
        case TAC_CONST_ULONG:
            rc->u.ulong_long_val = (unsigned long long)src->u.ulong_val;
            break;
        default:
            tac_free_const(rc);
            return NULL;
        }
        break;

    case TAC_INSTRUCTION_TRUNCATE:
        if (dst_kind >= 0 && const_is_integer_kind(src->kind))
            return make_int_const_val((Tac_ConstKind)dst_kind, const_to_uint64(src));
        switch (src->kind) {
        case TAC_CONST_LONG:
        case TAC_CONST_LONG_LONG:
            rc = tac_new_const(TAC_CONST_INT);
            rc->u.int_val =
                sign_narrow((uint64_t)const_to_int64(src), target_signed_bits(TAC_CONST_INT));
            break;
        case TAC_CONST_ULONG:
        case TAC_CONST_ULONG_LONG:
            rc = tac_new_const(TAC_CONST_UINT);
            rc->u.uint_val =
                unsigned_narrow(const_to_uint64(src), target_unsigned_bits(TAC_CONST_UINT));
            break;
        case TAC_CONST_INT:
            // A truncation to plain `char` cannot be distinguished from one to
            // `signed char` at this level, so pick the result kind by the target's
            // plain-char signedness: on an unsigned-char target the folded constant
            // becomes TAC_CONST_UCHAR and a following ZERO_EXTEND folds through the
            // existing unsigned-char path.  (Residual: an explicit `signed char`
            // constant on such a target shares this path; its later SIGN_EXTEND is
            // simply left unfolded and computed correctly by the backend at runtime.)
            if (char_const_signed()) {
                rc             = tac_new_const(TAC_CONST_SCHAR);
                rc->u.char_val = (int)(int8_t)src->u.int_val;
            } else {
                rc              = tac_new_const(TAC_CONST_UCHAR);
                rc->u.uchar_val = (unsigned char)src->u.int_val;
            }
            break;
        case TAC_CONST_UINT:
            rc              = tac_new_const(TAC_CONST_UCHAR);
            rc->u.uchar_val = (unsigned char)src->u.uint_val;
            break;
        default:
            return NULL;
        }
        break;

        /* ---- integer → floating-point ---- */

    case TAC_INSTRUCTION_INT_TO_DOUBLE:
        if (!const_is_integer_kind(src->kind))
            return NULL;
        rc               = tac_new_const(TAC_CONST_DOUBLE);
        rc->u.double_val = target_double_from_i64(const_to_int64(src));
        break;

    case TAC_INSTRUCTION_UINT_TO_DOUBLE:
        if (!const_is_integer_kind(src->kind))
            return NULL;
        rc               = tac_new_const(TAC_CONST_DOUBLE);
        rc->u.double_val = target_double_from_u64(const_to_uint64(src));
        break;

    case TAC_INSTRUCTION_INT_TO_FLOAT:
        if (!const_is_integer_kind(src->kind))
            return NULL;
        rc              = tac_new_const(TAC_CONST_FLOAT);
        rc->u.float_val = (double)(float)const_to_int64(src);
        break;

    case TAC_INSTRUCTION_UINT_TO_FLOAT:
        if (!const_is_integer_kind(src->kind))
            return NULL;
        rc              = tac_new_const(TAC_CONST_FLOAT);
        rc->u.float_val = (double)(float)const_to_uint64(src);
        break;

    case TAC_INSTRUCTION_INT_TO_LONG_DOUBLE:
        if (!const_is_integer_kind(src->kind))
            return NULL;
        rc                    = tac_new_const(TAC_CONST_LONG_DOUBLE);
        rc->u.long_double_val = ld_from_int(const_to_int64(src), false);
        break;

    case TAC_INSTRUCTION_UINT_TO_LONG_DOUBLE:
        if (!const_is_integer_kind(src->kind))
            return NULL;
        rc                    = tac_new_const(TAC_CONST_LONG_DOUBLE);
        rc->u.long_double_val = ld_from_int((int64_t)const_to_uint64(src), true);
        break;

        /* ---- floating-point → integer (truncate toward zero) ---- */

    case TAC_INSTRUCTION_DOUBLE_TO_INT:
    case TAC_INSTRUCTION_FLOAT_TO_INT: {
        double d;
        if (src->kind == TAC_CONST_FLOAT)
            d = src->u.float_val;
        else if (src->kind == TAC_CONST_DOUBLE)
            d = src->u.double_val;
        else
            return NULL;
        if (dst_kind >= 0)
            return make_int_const_val((Tac_ConstKind)dst_kind, (uint64_t)(int64_t)d);
        rc            = tac_new_const(TAC_CONST_INT);
        rc->u.int_val = sign_narrow((uint64_t)(int64_t)d, target_signed_bits(TAC_CONST_INT));
        break;
    }

    case TAC_INSTRUCTION_DOUBLE_TO_UINT:
    case TAC_INSTRUCTION_FLOAT_TO_UINT: {
        double d;
        if (src->kind == TAC_CONST_FLOAT)
            d = src->u.float_val;
        else if (src->kind == TAC_CONST_DOUBLE)
            d = src->u.double_val;
        else
            return NULL;
        if (dst_kind >= 0)
            return make_int_const_val((Tac_ConstKind)dst_kind, (uint64_t)d);
        rc             = tac_new_const(TAC_CONST_UINT);
        rc->u.uint_val = unsigned_narrow((uint64_t)d, target_unsigned_bits(TAC_CONST_UINT));
        break;
    }

    // These two carry no destination kind: the result may be an int or a long. So
    // the folder answers only when it fits int (unsigned int), where the two agree,
    // and leaves a wider one to the run time rather than wrap it to the int width.
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_INT: {
        if (src->kind != TAC_CONST_LONG_DOUBLE)
            return NULL;
        int64_t v = f128_to_i64(ld_value(src), 64);
        if (sign_narrow((uint64_t)v, target_signed_bits(TAC_CONST_INT)) != v)
            return NULL;
        rc            = tac_new_const(TAC_CONST_INT);
        rc->u.int_val = v;
        break;
    }

    case TAC_INSTRUCTION_LONG_DOUBLE_TO_UINT: {
        if (src->kind != TAC_CONST_LONG_DOUBLE)
            return NULL;
        uint64_t u = f128_to_u64(ld_value(src), 64);
        if (unsigned_narrow(u, target_unsigned_bits(TAC_CONST_UINT)) != u)
            return NULL;
        rc             = tac_new_const(TAC_CONST_UINT);
        rc->u.uint_val = u;
        break;
    }

        /* ---- floating-point ↔ floating-point ---- */

    case TAC_INSTRUCTION_FLOAT_TO_DOUBLE:
        if (src->kind != TAC_CONST_FLOAT)
            return NULL;
        rc               = tac_new_const(TAC_CONST_DOUBLE);
        rc->u.double_val = src->u.float_val; // float_val already stored as double
        break;

    case TAC_INSTRUCTION_DOUBLE_TO_FLOAT:
        if (src->kind != TAC_CONST_DOUBLE)
            return NULL;
        rc              = tac_new_const(TAC_CONST_FLOAT);
        rc->u.float_val = round_float(src->u.double_val);
        break;

    case TAC_INSTRUCTION_FLOAT_TO_LONG_DOUBLE:
        if (src->kind != TAC_CONST_FLOAT)
            return NULL;
        rc                    = tac_new_const(TAC_CONST_LONG_DOUBLE);
        rc->u.long_double_val = f128_from_double(src->u.float_val);
        break;

    case TAC_INSTRUCTION_LONG_DOUBLE_TO_FLOAT:
        if (src->kind != TAC_CONST_LONG_DOUBLE)
            return NULL;
        rc              = tac_new_const(TAC_CONST_FLOAT);
        rc->u.float_val = round_float(ld_is_double() ? f128_to_double(ld_value(src))
                                                     : (double)f128_to_float(ld_value(src)));
        break;

    case TAC_INSTRUCTION_DOUBLE_TO_LONG_DOUBLE:
        if (src->kind != TAC_CONST_DOUBLE)
            return NULL;
        rc                    = tac_new_const(TAC_CONST_LONG_DOUBLE);
        rc->u.long_double_val = f128_from_double(src->u.double_val);
        break;

    case TAC_INSTRUCTION_LONG_DOUBLE_TO_DOUBLE:
        if (src->kind != TAC_CONST_LONG_DOUBLE)
            return NULL;
        rc               = tac_new_const(TAC_CONST_DOUBLE);
        rc->u.double_val = f128_to_double(ld_value(src));
        break;

    default:
        return NULL;
    }

    Tac_Val *rv    = tac_new_val(TAC_VAL_CONSTANT);
    rv->u.constant = rc;
    return rv;
}

// Walk the flat instruction list once, folding every instruction whose operands
// are all constant. Returns the (possibly new) head of the list.
//
// For Unary/Binary/conversion instructions the replacement pattern is uniform
// and worth describing once: build a fresh Copy whose source is the folded
// constant and whose destination is *stolen* from the original instruction (the
// dst Tac_Val is moved, not copied). Before freeing the original we NULL out the
// stolen dst field so tac_free_instruction does not free it (it now belongs to
// the Copy), and NULL out `cur->next` so the recursive free does not cascade
// into the rest of the list. The Copy is then spliced in where the original was.
Tac_Instruction *constant_fold_typed(Tac_Instruction *body, const Tac_TopLevel *fn);

Tac_Instruction *constant_fold(Tac_Instruction *body)
{
    return constant_fold_typed(body, NULL);
}

// constant_fold, with `fn`'s params and locals for the types of its variables.
Tac_Instruction *constant_fold_typed(Tac_Instruction *body, const Tac_TopLevel *fn)
{
    Tac_Instruction *prev = NULL;
    Tac_Instruction *cur  = body;

    while (cur) {
        Tac_Instruction *next = cur->next;

        // Never fold or rewrite a volatile access: it must execute verbatim.
        if (cur->is_volatile) {
            prev = cur;
            cur  = next;
            continue;
        }

        // Unary with a constant operand → Copy of the folded result.
        if (cur->kind == TAC_INSTRUCTION_UNARY && cur->u.unary.src->kind == TAC_VAL_CONSTANT) {
            Tac_Val *folded = fold_unary_const(cur->u.unary.op, cur->u.unary.src->u.constant);
            if (folded) {
                opt_trace_instr("[const-fold] unary fold:", cur);
                Tac_Instruction *copy = tac_new_instruction(TAC_INSTRUCTION_COPY);
                copy->u.copy.src      = folded;
                copy->u.copy.dst      = cur->u.unary.dst; // steal dst
                copy->next            = next;

                cur->u.unary.dst = NULL; // prevent double-free in tac_free_instruction
                cur->next        = NULL; // prevent cascade-free
                tac_free_instruction(cur);

                if (prev)
                    prev->next = copy;
                else
                    body = copy;

                opt_trace_instr("[const-fold]          →", copy);
                prev = copy;
                cur  = next;
                continue;
            }
        }

        // Binary with two constant operands → Copy of the folded result.
        if (cur->kind == TAC_INSTRUCTION_BINARY && cur->u.binary.src1->kind == TAC_VAL_CONSTANT &&
            cur->u.binary.src2->kind == TAC_VAL_CONSTANT) {
            Tac_Val *folded = fold_binary_const(cur->u.binary.op, cur->u.binary.src1->u.constant,
                                                cur->u.binary.src2->u.constant,
                                                var_const_kind(fn, cur->u.binary.dst));
            if (folded) {
                opt_trace_instr("[const-fold] binary fold:", cur);
                Tac_Instruction *copy = tac_new_instruction(TAC_INSTRUCTION_COPY);
                copy->u.copy.src      = folded;
                copy->u.copy.dst      = cur->u.binary.dst; // steal dst
                copy->next            = next;

                cur->u.binary.dst = NULL; // prevent double-free in tac_free_instruction
                cur->next         = NULL; // prevent cascade-free
                tac_free_instruction(cur);

                if (prev)
                    prev->next = copy;
                else
                    body = copy;

                opt_trace_instr("[const-fold]           →", copy);
                prev = copy;
                cur  = next;
                continue;
            }
        }

        // A comparison of a constant with a variable → the mirrored comparison, the
        // constant second, where the code generators take an immediate. A rotated loop's
        // guard compares the initial value with the bound, so this is one of the loop
        // optimizations, off where they are.
        if (cur->kind == TAC_INSTRUCTION_BINARY && !target_config->no_loop_opt &&
            cur->u.binary.src1->kind == TAC_VAL_CONSTANT &&
            cur->u.binary.src2->kind != TAC_VAL_CONSTANT) {
            Tac_BinaryOperator m = mirror_comparison(cur->u.binary.op);
            if (m != (Tac_BinaryOperator)-1) {
                opt_trace_instr("[const-fold] mirror:", cur);
                Tac_Val *t         = cur->u.binary.src1;
                cur->u.binary.src1 = cur->u.binary.src2;
                cur->u.binary.src2 = t;
                cur->u.binary.op   = m;
                opt_trace_instr("[const-fold]       →", cur);
            }
        }

        // Any conversion of a constant source → Copy of the new constant.
        // All 14 conversions share the sign_extend {src, dst} layout.
        if (is_conversion(cur->kind) && cur->u.sign_extend.src->kind == TAC_VAL_CONSTANT) {
            // dst_kind is meaningful only for the integer-width and float→integer
            // conversions; the others' result kind is fixed by the op.
            int dst_kind = (cur->kind == TAC_INSTRUCTION_SIGN_EXTEND ||
                            cur->kind == TAC_INSTRUCTION_TRUNCATE ||
                            cur->kind == TAC_INSTRUCTION_ZERO_EXTEND ||
                            cur->kind == TAC_INSTRUCTION_DOUBLE_TO_INT ||
                            cur->kind == TAC_INSTRUCTION_DOUBLE_TO_UINT ||
                            cur->kind == TAC_INSTRUCTION_FLOAT_TO_INT ||
                            cur->kind == TAC_INSTRUCTION_FLOAT_TO_UINT)
                               ? cur->u.sign_extend.dst_kind
                               : -1;
            Tac_Val *folded =
                fold_conversion(cur->kind, cur->u.sign_extend.src->u.constant, dst_kind);
            if (folded) {
                opt_trace_instr("[const-fold] conversion fold:", cur);
                Tac_Instruction *copy = tac_new_instruction(TAC_INSTRUCTION_COPY);
                copy->u.copy.src      = folded;
                copy->u.copy.dst      = cur->u.sign_extend.dst; // steal dst
                copy->next            = next;

                cur->u.sign_extend.dst = NULL; // prevent double-free
                cur->next              = NULL; // prevent cascade-free
                tac_free_instruction(cur);

                if (prev)
                    prev->next = copy;
                else
                    body = copy;

                opt_trace_instr("[const-fold]              →", copy);
                prev = copy;
                cur  = next;
                continue;
            }
        }

        // Conditional jump with a constant condition → resolve it statically.
        // (JumpIfZero and JumpIfNotZero share the jump_if_zero union layout.)
        // If the branch is always taken, replace it with an unconditional Jump
        // to the same target; if never taken, delete it. Either way the block
        // structure simplifies, which gives unreachable-code elimination new
        // dead blocks to remove on the next CFG pass.
        if ((cur->kind == TAC_INSTRUCTION_JUMP_IF_ZERO ||
             cur->kind == TAC_INSTRUCTION_JUMP_IF_NOT_ZERO) &&
            cur->u.jump_if_zero.condition->kind == TAC_VAL_CONSTANT) {
            bool is_zero = const_is_zero(cur->u.jump_if_zero.condition->u.constant);
            bool take    = (cur->kind == TAC_INSTRUCTION_JUMP_IF_ZERO) ? is_zero : !is_zero;

            if (take) {
                // Always taken: build a Jump, stealing the target label string.
                opt_trace_instr("[const-fold] cond-jump always taken:", cur);
                OPT_TRACE("[const-fold]   → unconditional jump to %s\n",
                          cur->u.jump_if_zero.target);
                Tac_Instruction *jmp = tac_new_instruction(TAC_INSTRUCTION_JUMP);
                jmp->u.jump.target   = cur->u.jump_if_zero.target; // steal
                jmp->next            = next;

                tac_free_val(cur->u.jump_if_zero.condition);
                cur->u.jump_if_zero.condition = NULL;
                cur->u.jump_if_zero.target    = NULL; // stolen; don't free it
                cur->next                     = NULL;
                tac_free_instruction(cur);

                if (prev)
                    prev->next = jmp;
                else
                    body = jmp;
                prev = jmp;
            } else {
                // Never taken: unlink and free the conditional jump entirely.
                opt_trace_instr("[const-fold] cond-jump never taken → deleted:", cur);
                if (prev)
                    prev->next = next;
                else
                    body = next;
                cur->next = NULL;
                tac_free_instruction(cur);
            }
            cur = next;
            continue;
        }

        // A jump table with a constant index → a Jump to the one target it picks.
        if (cur->kind == TAC_INSTRUCTION_JUMP_TABLE &&
            cur->u.jump_table.index->kind == TAC_VAL_CONSTANT) {
            uint64_t k         = 0;
            const Tac_Const *c = cur->u.jump_table.index->u.constant;
            if (!const_to_index(c, &k)) {
                prev = cur;
                cur  = next;
                continue;
            }
            char **slot = k < (uint64_t)cur->u.jump_table.count ? &cur->u.jump_table.targets[k]
                                                                : &cur->u.jump_table.default_target;
            opt_trace_instr("[const-fold] jump table on a constant:", cur);
            Tac_Instruction *jmp = tac_new_instruction(TAC_INSTRUCTION_JUMP);
            jmp->u.jump.target   = *slot; // steal
            *slot                = xstrdup("");
            jmp->next            = next;
            cur->next            = NULL;
            tac_free_instruction(cur);
            if (prev)
                prev->next = jmp;
            else
                body = jmp;
            prev = jmp;
            cur  = next;
            continue;
        }

        prev = cur;
        cur  = next;
    }

    return body;
}
