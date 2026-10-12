#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "semantic.h"
#include "symtab.h"
#include "target.h"

//
// Convert literal to int64
//
//
// The value of a float or double literal, as the target's double holds it: one from the
// source takes strtof's value where double is IEEE single (rounding the strtod one again
// could round twice).
//
static double literal_real(const Literal *lit)
{
    if (lit->kind == LITERAL_DOUBLE && lit->spelling && target_double_is_single())
        return lit->single_val;
    return lit->kind == LITERAL_DOUBLE ? target_double_round(lit->u.real_val) : lit->u.real_val;
}

int64_t literal_to_int64(const Literal *lit)
{
    switch (lit->kind) {
    case LITERAL_CHAR:
        return (int64_t)lit->u.char_val;
    case LITERAL_INT:
        return (int64_t)lit->u.int_val;
    case LITERAL_LONG:
        return (int64_t)lit->u.long_val;
    case LITERAL_LONG_LONG:
        return (int64_t)lit->u.long_long_val;
    case LITERAL_UINT:
        return (int64_t)lit->u.uint_val;
    case LITERAL_ULONG:
        return (int64_t)lit->u.ulong_val;
    case LITERAL_ULONG_LONG:
        return (int64_t)lit->u.ulong_long_val;
    case LITERAL_FLOAT:
    case LITERAL_DOUBLE:
        return (int64_t)literal_real(lit);
    case LITERAL_LONG_DOUBLE:
        return f128_to_i64(lit->u.long_double_val, 64);
    case LITERAL_STRING:
        internal_error("literal_to_int64: Cannot convert string %s", lit->u.string_val);
    case LITERAL_ENUM:
        internal_error("literal_to_int64: Cannot convert enum %s", lit->u.enum_const);
    default:
        internal_error("literal_to_int64: Unknown kind %d", lit->kind);
    }
}

//
// Convert literal to uint64
//
uint64_t literal_to_uint64(const Literal *lit)
{
    switch (lit->kind) {
    case LITERAL_CHAR:
        return (uint64_t)lit->u.char_val;
    case LITERAL_INT:
        return (uint64_t)lit->u.int_val;
    case LITERAL_LONG:
        return (uint64_t)lit->u.long_val;
    case LITERAL_LONG_LONG:
        return (uint64_t)lit->u.long_long_val;
    case LITERAL_UINT:
        return (uint64_t)lit->u.uint_val;
    case LITERAL_ULONG:
        return (uint64_t)lit->u.ulong_val;
    case LITERAL_ULONG_LONG:
        return (uint64_t)lit->u.ulong_long_val;
    case LITERAL_FLOAT:
    case LITERAL_DOUBLE:
        return (uint64_t)literal_real(lit);
    case LITERAL_LONG_DOUBLE:
        return f128_to_u64(lit->u.long_double_val, 64);
    case LITERAL_STRING:
        internal_error("literal_to_uint64: Cannot convert string %s", lit->u.string_val);
    case LITERAL_ENUM:
        internal_error("literal_to_uint64: Cannot convert enum %s", lit->u.enum_const);
    default:
        internal_error("literal_to_uint64: Unknown kind %d", lit->kind);
    }
}

//
// Convert literal to double
//
double literal_to_double(const Literal *lit)
{
    switch (lit->kind) {
    case LITERAL_CHAR:
        return (double)lit->u.char_val;
    case LITERAL_INT:
        return target_double_from_i64(lit->u.int_val);
    case LITERAL_LONG:
        return target_double_from_i64(lit->u.long_val);
    case LITERAL_LONG_LONG:
        return target_double_from_i64(lit->u.long_long_val);
    case LITERAL_UINT:
        return target_double_from_u64(lit->u.uint_val);
    case LITERAL_ULONG:
        return target_double_from_u64(lit->u.ulong_val);
    case LITERAL_ULONG_LONG:
        return target_double_from_u64(lit->u.ulong_long_val);
    case LITERAL_FLOAT:
    case LITERAL_DOUBLE:
        return literal_real(lit);
    case LITERAL_LONG_DOUBLE:
        return target_double_is_single() ? (double)f128_to_float(lit->u.long_double_val)
                                         : f128_to_double(lit->u.long_double_val);
    case LITERAL_STRING:
        internal_error("literal_to_double: Cannot convert string %s", lit->u.string_val);
    case LITERAL_ENUM:
        internal_error("literal_to_double: Cannot convert enum %s", lit->u.enum_const);
    default:
        internal_error("literal_to_double: Unknown kind %d", lit->kind);
    }
}

Float128 literal_to_long_double(const Literal *lit)
{
    switch (lit->kind) {
    case LITERAL_CHAR:
        return f128_from_i64(lit->u.char_val);
    case LITERAL_INT:
        return f128_from_i64(lit->u.int_val);
    case LITERAL_LONG:
        return f128_from_i64(lit->u.long_val);
    case LITERAL_LONG_LONG:
        return f128_from_i64(lit->u.long_long_val);
    case LITERAL_UINT:
        return f128_from_u64(lit->u.uint_val);
    case LITERAL_ULONG:
        return f128_from_u64(lit->u.ulong_val);
    case LITERAL_ULONG_LONG:
        return f128_from_u64(lit->u.ulong_long_val);
    case LITERAL_FLOAT:
    case LITERAL_DOUBLE:
        return f128_from_double(literal_real(lit));
    case LITERAL_LONG_DOUBLE:
        return lit->u.long_double_val;
    case LITERAL_STRING:
        internal_error("literal_to_long_double: Cannot convert string %s", lit->u.string_val);
    case LITERAL_ENUM:
        internal_error("literal_to_long_double: Cannot convert enum %s", lit->u.enum_const);
    default:
        internal_error("literal_to_long_double: Unknown kind %d", lit->kind);
    }
}

void check_int_literal_width(const Literal *lit)
{
    if (lit->spelling)
        return; // an integer constant: type_int_literal gives it a type that holds it
    int width = (int)target_config->int_size * 8;
    if (lit->kind == LITERAL_INT &&
        unsigned_narrow((uint64_t)lit->u.int_val, width) != (uint64_t)lit->u.int_val &&
        sign_narrow((uint64_t)lit->u.int_val, target_config->int_bits) != lit->u.int_val)
        fatal_error("character constant too long for type int");
    if (lit->kind == LITERAL_UINT && unsigned_narrow(lit->u.uint_val, width) != lit->u.uint_val)
        fatal_error("character constant too long for type unsigned int");
}

//
// The value of an integer literal of kind `kind`, as its 64-bit pattern.
//
static uint64_t int_literal_bits(const Literal *lit)
{
    switch (lit->kind) {
    case LITERAL_INT:
        return (uint64_t)lit->u.int_val;
    case LITERAL_LONG:
        return (uint64_t)lit->u.long_val;
    case LITERAL_LONG_LONG:
        return (uint64_t)lit->u.long_long_val;
    case LITERAL_UINT:
        return lit->u.uint_val;
    case LITERAL_ULONG:
        return (uint64_t)lit->u.ulong_val;
    case LITERAL_ULONG_LONG:
        return (uint64_t)lit->u.ulong_long_val;
    default:
        internal_error("int_literal_bits: not an integer literal, kind %d", lit->kind);
    }
}

//
// Does the value `v` of an integer constant fit the target's type `kind`?
//
static bool int_literal_fits(uint64_t v, LiteralKind kind)
{
    int bits;
    switch (kind) {
    case LITERAL_INT:
        bits = target_config->int_bits;
        break;
    case LITERAL_LONG:
        bits = target_config->long_bits;
        break;
    case LITERAL_LONG_LONG:
        bits = target_config->llong_bits;
        break;
    case LITERAL_UINT:
        return unsigned_narrow(v, (int)target_config->int_size * 8) == v;
    case LITERAL_ULONG:
        return unsigned_narrow(v, (int)target_config->long_size * 8) == v;
    default:
        return true; // unsigned long long holds any 64-bit constant
    }
    return bits >= 64 ? v <= INT64_MAX : v <= ((uint64_t)1 << (bits - 1)) - 1;
}

void type_char_literal(Literal *lit)
{
    if ((lit->spelling & LITERAL_CHAR_BYTE) && lit->kind == LITERAL_INT &&
        target_config->char_signed)
        lit->u.int_val = (int8_t)lit->u.int_val;
}

void type_int_literal(Literal *lit)
{
    type_char_literal(lit);
    if (!(lit->spelling & LITERAL_SPELLED) || lit->kind > LITERAL_ULONG_LONG)
        return;

    // C11 §6.4.4.1p5: the first type of the suffix's list that can represent the value.
    // Octal and hexadecimal constants may take the unsigned types too.
    static const LiteralKind dec_none[] = { LITERAL_INT, LITERAL_LONG, LITERAL_LONG_LONG };
    static const LiteralKind hex_none[] = { LITERAL_INT,   LITERAL_UINT,      LITERAL_LONG,
                                            LITERAL_ULONG, LITERAL_LONG_LONG, LITERAL_ULONG_LONG };
    static const LiteralKind dec_l[]    = { LITERAL_LONG, LITERAL_LONG_LONG };
    static const LiteralKind hex_l[]    = { LITERAL_LONG, LITERAL_ULONG, LITERAL_LONG_LONG,
                                            LITERAL_ULONG_LONG };
    static const LiteralKind dec_ll[]   = { LITERAL_LONG_LONG };
    static const LiteralKind hex_ll[]   = { LITERAL_LONG_LONG, LITERAL_ULONG_LONG };
    static const LiteralKind u_none[]   = { LITERAL_UINT, LITERAL_ULONG, LITERAL_ULONG_LONG };
    static const LiteralKind u_l[]      = { LITERAL_ULONG, LITERAL_ULONG_LONG };
    static const LiteralKind u_ll[]     = { LITERAL_ULONG_LONG };
    const LiteralKind *list;
    size_t n;
    bool dec = lit->spelling & LITERAL_DECIMAL;
    if (lit->spelling & LITERAL_SUFFIX_U) {
        if (lit->spelling & LITERAL_SUFFIX_LL) {
            list = u_ll, n = 1;
        } else if (lit->spelling & LITERAL_SUFFIX_L) {
            list = u_l, n = 2;
        } else {
            list = u_none, n = 3;
        }
    } else if (lit->spelling & LITERAL_SUFFIX_LL) {
        list = dec ? dec_ll : hex_ll, n = dec ? 1 : 2;
    } else if (lit->spelling & LITERAL_SUFFIX_L) {
        list = dec ? dec_l : hex_l, n = dec ? 2 : 4;
    } else {
        list = dec ? dec_none : hex_none, n = dec ? 3 : 6;
    }

    // A value that fits none of them takes the last (C11 leaves it undefined; clang
    // warns): long long for a decimal, which wraps as before.
    uint64_t v       = int_literal_bits(lit);
    LiteralKind kind = list[n - 1];
    for (size_t i = 0; i < n; i++) {
        if (int_literal_fits(v, list[i])) {
            kind = list[i];
            break;
        }
    }
    lit->kind = kind;
    switch (kind) {
    case LITERAL_INT:
        lit->u.int_val = (int64_t)v;
        break;
    case LITERAL_LONG:
        lit->u.long_val = (long)v;
        break;
    case LITERAL_LONG_LONG:
        lit->u.long_long_val = (long long)v;
        break;
    case LITERAL_UINT:
        lit->u.uint_val = v;
        break;
    case LITERAL_ULONG:
        lit->u.ulong_val = (unsigned long)v;
        break;
    default:
        lit->u.ulong_long_val = v;
        break;
    }
}

Tac_StaticInit *new_static_init_int(size_t size, bool sign, uint64_t bits)
{
    Tac_StaticInit *result;
    if (size == 2) {
        // int, unsigned int and pointers on AVR
        result = tac_new_static_init(sign ? TAC_STATIC_INIT_I16 : TAC_STATIC_INIT_U16);
        if (sign)
            result->u.short_val = (int16_t)bits;
        else
            result->u.ushort_val = (uint16_t)bits;
    } else if (size == 4) {
        result = tac_new_static_init(sign ? TAC_STATIC_INIT_I32 : TAC_STATIC_INIT_U32);
        if (sign)
            result->u.int_val = (int32_t)bits;
        else
            result->u.uint_val = (uint32_t)bits;
    } else {
        result = tac_new_static_init(sign ? TAC_STATIC_INIT_I64 : TAC_STATIC_INIT_U64);
        if (sign)
            result->u.long_val = (int64_t)bits;
        else
            result->u.ulong_val = bits;
    }
    return result;
}

//
// Convert literal to given arithmetic type and return as Tac_StaticInit.
//
Tac_StaticInit *new_static_init_from_literal(const Type *target_type, const Literal *lit)
{
    if (!is_arithmetic(target_type)) {
        internal_error("invalid static initializer for type %d", target_type->kind);
    }

    Tac_StaticInit *result = NULL;
    switch (target_type->kind) {
    case TYPE_BOOL:
        // C11 §6.3.1.2: a scalar converted to _Bool is 0 or 1.  The init slot follows the
        // type's storage, which ast_type_to_tac_type carries in the same widths: a
        // word-sized _Bool uses int's slot, a byte-sized one unsigned char's (only the
        // I8/U8 kinds are packed sub-word by the BESM-6 static emitter).
        if (get_size(target_type) == 1) {
            result              = tac_new_static_init(TAC_STATIC_INIT_U8);
            result->u.uchar_val = (literal_to_int64(lit) != 0);
        } else {
            result = new_static_init_int(get_size(target_type), true, literal_to_int64(lit) != 0);
        }
        break;

    case TYPE_CHAR:
    case TYPE_SCHAR:
        result             = tac_new_static_init(TAC_STATIC_INIT_I8);
        result->u.char_val = (int8_t)literal_to_int64(lit);
        break;

    case TYPE_UCHAR:
        result              = tac_new_static_init(TAC_STATIC_INIT_U8);
        result->u.uchar_val = (uint8_t)literal_to_int64(lit);
        break;

    case TYPE_SHORT:
        result              = tac_new_static_init(TAC_STATIC_INIT_I16);
        result->u.short_val = (int16_t)literal_to_int64(lit);
        break;

    case TYPE_INT:
    case TYPE_ENUM:
        // An enumerated type is int-sized, int-aligned and signed everywhere else
        // (get_size/get_alignment/is_signed, and ast_type_to_tac_type maps it to
        // TAC_TYPE_INT), so it shares int's representation here too.
        result = new_static_init_int(get_size(target_type), true, literal_to_int64(lit));
        break;

    case TYPE_USHORT:
        result               = tac_new_static_init(TAC_STATIC_INIT_U16);
        result->u.ushort_val = (uint16_t)literal_to_int64(lit);
        break;

    case TYPE_LONG:
    case TYPE_LONG_LONG:
        result = new_static_init_int(get_size(target_type), true, literal_to_int64(lit));
        break;

    case TYPE_UINT:
    case TYPE_ULONG:
    case TYPE_ULONG_LONG:
        result = new_static_init_int(get_size(target_type), false, literal_to_uint64(lit));
        break;

    case TYPE_FLOAT:
        result              = tac_new_static_init(TAC_STATIC_INIT_FLOAT);
        result->u.float_val = literal_to_double(lit);
        break;

    case TYPE_DOUBLE:
        result               = tac_new_static_init(TAC_STATIC_INIT_DOUBLE);
        result->u.double_val = literal_to_double(lit);
        break;

    case TYPE_LONG_DOUBLE:
        result                    = tac_new_static_init(TAC_STATIC_INIT_LONG_DOUBLE);
        result->u.long_double_val = literal_to_long_double(lit);
        break;
    default:
        fatal_error("cannot initialize an object of type '%s' with a constant",
                    type_to_c(target_type));
    }
    return result;
}
