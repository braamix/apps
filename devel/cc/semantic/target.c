#include "target.h"

#include <stdio.h>
#include <string.h>

#include "tac.h"

//
// The <besm6.h> intrinsics with an immediate first argument (backend/besm6/Besm6_Intrinsics.md
// §3.3, §3.4, §5).
//
static const ImmediateArg besm6_immediate_args[] = {
    // `op` *is* the extracode's opcode; only 050..077 are extracodes, anything else names a
    // different instruction entirely.
    { "__besm6_extracode", "opcode", "is not an extracode (050..077)", 050, 077 },
    // The mask of the register-0 `vtm` mode write, and the halt code of `033`: both ride in
    // the instruction's own 15-bit address field.
    { "__besm6_maskpsw", "mask", "does not fit the 15-bit address field", 0, 077777 },
    { "__besm6_stop", "halt code", "does not fit the 15-bit address field", 0, 077777 },
    {},
};

// clang-format off
static const Target targets[] = {
    // name
    //            bool_size   bool_align
    //            short_size  short_align
    //            int_size    int_align
    //            long_size   long_align
    //            llong_size  llong_align
    //            float_size  float_align
    //            double_size double_align
    //            ldouble_size ldouble_align
    //            pointer_size pointer_align

    { "avr",
      1, 1,   // _Bool
      2, 1,   // short
      2, 1,   // int
      4, 1,   // long
      8, 1,   // long long
      4, 1,   // float
      4, 1,   // double  (same as float in avr-gcc default mode)
      4, 1,   // long double (same as float)
      2, 1,   // pointer (16-bit data address; a function pointer is a word address)
      16, 16, 32, 64, // signed bits: short int long llong
      1,   // plain char signed (avr-gcc, clang)
      0,   // signed >> arithmetic
      1,   // aggregate_align (1)
      8,        // struct_return_max: up to 8 bytes in r18-r25; larger through a
                // hidden pointer in r24:r25, the first argument
      0,        // struct_args_split
      NULL,     // immediate_args
      NULL,     // va_class: every variadic argument is on the stack, va_arg a pointer walk
      24,       // ldouble_mant_dig: long double is double, IEEE single
      0,        // hw_sqrt: no FP hardware
      24,       // double_mant_dig: double is IEEE single
      0,        // no_loop_opt
      0,        // little-endian
      BITFIELD_SYSV,
      16,       // bitfield_access_bits: clang's access units, at most an int
      0,        // bitfield_unit_per_field
      0,        // jump_tables
      0 },      // no_coroutines

    { "msp430",
      1, 1,   // _Bool
      2, 2,   // short
      2, 2,   // int
      4, 2,   // long
      8, 2,   // long long
      4, 2,   // float
      8, 2,   // double
      8, 2,   // long double (same as double)
      2, 2,   // pointer (16-bit, 4 bytes on MSP430X)
      16, 16, 32, 64, // signed bits
      0,   // plain char unsigned (GCC and clang: __CHAR_UNSIGNED__)
      0,   // signed >> arithmetic
      1,   // aggregate_align (1)
      0,        // struct_return_max: every struct and union through a hidden
                // pointer in R12, the first argument, even a 1-byte one
      0,        // struct_args_split
      NULL,     // immediate_args
      tac_msp430_class, // va_class: every variadic argument is on the stack, va_arg a
                        // pointer walk that follows a structure's address
      0,        // ldouble_mant_dig: long double is double, binary64
      0,        // hw_sqrt: no FP hardware
      0,        // double_mant_dig: binary64
      0,        // no_loop_opt
      0,        // little-endian
      BITFIELD_SYSV,
      0,        // bitfield_access_bits
      0,        // bitfield_unit_per_field
      0,        // jump_tables
      0 },      // no_coroutines

    { "arm32",
      1, 1,   // _Bool
      2, 2,   // short
      4, 4,   // int
      4, 4,   // long (ILP32: same size as int)
      8, 8,   // long long
      4, 4,   // float
      8, 8,   // double
      8, 8,   // long double (same as double on ARM EABI)
      4, 4,   // pointer
      16, 32, 32, 64, // signed bits
      0,   // plain char unsigned (ARM EABI)
      0,   // signed >> arithmetic
      1,   // aggregate_align (1)
      SIZE_MAX, // struct_return_max: never lowered by the front end; the backend
                // returns an HFA in VFP registers although it is wider than 4 bytes
      0,        // struct_args_split
      NULL,     // immediate_args
      NULL,     // va_class: va_arg is a pointer walk, no argument classes
      0,        // ldouble_mant_dig
      1,        // hw_sqrt: vsqrt.f64
      0,        // double_mant_dig: binary64
      0,        // no_loop_opt
      0,        // little-endian
      BITFIELD_AAPCS,
      0,        // bitfield_access_bits
      0,        // bitfield_unit_per_field
      0,        // jump_tables
      0 },      // no_coroutines

    { "aarch64",
      1, 1,   // _Bool
      2, 2,   // short
      4, 4,   // int
      8, 8,   // long (LP64)
      8, 8,   // long long
      4, 4,   // float
      8, 8,   // double
     16,16,   // long double (IEEE 754 binary128)
      8, 8,   // pointer
      16, 32, 64, 64, // signed bits
      0,   // plain char unsigned (AAPCS64)
      0,   // signed >> arithmetic
      1,   // aggregate_align (1)
      SIZE_MAX, // struct_return_max: never lowered by the front end; the backend
                // passes a large result's address in x8, not as an argument
      0,        // struct_args_split
      NULL,     // immediate_args
      tac_aapcs64_class, // va_class
      0,        // ldouble_mant_dig
      1,        // hw_sqrt: fsqrt
      0,        // double_mant_dig: binary64
      0,        // no_loop_opt
      0,        // little-endian
      BITFIELD_AAPCS,
      0,        // bitfield_access_bits
      0,        // bitfield_unit_per_field
      0,        // jump_tables
      0 },      // no_coroutines

    { "x86_64",
      1, 1,   // _Bool
      2, 2,   // short
      4, 4,   // int
      8, 8,   // long (LP64)
      8, 8,   // long long
      4, 4,   // float
      8, 8,   // double
     16,16,   // long double (x87 80-bit value in 16-byte slot)
      8, 8,   // pointer
      16, 32, 64, 64, // signed bits
      1,   // plain char signed (x86-64 System V)
      0,   // signed >> arithmetic
      1,   // aggregate_align (1)
      SIZE_MAX, // struct_return_max: never lowered by the front end; the backend
                // classifies a result by eightbyte, and returns a large result's
                // address (passed in rdi) in rax
      0,        // struct_args_split
      NULL,     // immediate_args
      tac_sysv64_class, // va_class
      64,       // ldouble_mant_dig: the x87 extended format
      1,        // hw_sqrt: sqrtsd
      0,        // double_mant_dig: binary64
      0,        // no_loop_opt
      0,        // little-endian
      BITFIELD_SYSV,
      0,        // bitfield_access_bits
      0,        // bitfield_unit_per_field
      0,        // jump_tables
      0 },      // no_coroutines

    { "riscv32",
      1, 1,   // _Bool
      2, 2,   // short
      4, 4,   // int
      4, 4,   // long (ILP32: same size as int)
      8, 8,   // long long
      4, 4,   // float
      8, 8,   // double
     16,16,   // long double (IEEE 754 binary128, software)
      4, 4,   // pointer
      16, 32, 32, 64, // signed bits
      0,   // plain char unsigned (RISC-V ABI)
      0,   // signed >> arithmetic
      1,   // aggregate_align (1)
      16,   // struct_return_max: ILP32D returns {double, double} in fa0/fa1; the
            // backend passes the hidden pointer for the other structs over 8 bytes
      0,        // struct_args_split
      NULL,     // immediate_args
      NULL,     // va_class
      0,        // ldouble_mant_dig
      1,        // hw_sqrt: fsqrt.d
      0,        // double_mant_dig
      0,        // no_loop_opt
      0,        // little-endian
      BITFIELD_SYSV,
      0,        // bitfield_access_bits
      1,        // bitfield_unit_per_field: the FP calling convention counts bit-fields
      0,        // jump_tables
      0 },      // no_coroutines

    { "riscv64",
      1, 1,   // _Bool
      2, 2,   // short
      4, 4,   // int
      8, 8,   // long (LP64)
      8, 8,   // long long
      4, 4,   // float
      8, 8,   // double
     16,16,   // long double (IEEE 754 binary128, software)
      8, 8,   // pointer
      16, 32, 64, 64, // signed bits
      0,   // plain char unsigned (RISC-V ABI)
      0,   // signed >> arithmetic
      1,   // aggregate_align (1)
      16,       // struct_return_max: two pointers, a0/a1
      0,        // struct_args_split
      NULL,     // immediate_args
      NULL,     // va_class
      0,        // ldouble_mant_dig
      1,        // hw_sqrt: fsqrt.d
      0,        // double_mant_dig
      0,        // no_loop_opt
      0,        // little-endian
      BITFIELD_SYSV,
      0,        // bitfield_access_bits
      1,        // bitfield_unit_per_field: the FP calling convention counts bit-fields
      0,        // jump_tables
      0 },      // no_coroutines

    { "mmix",
      1, 1,   // _Bool
      2, 2,   // short
      4, 4,   // int
      8, 8,   // long (LP64)
      8, 8,   // long long
      4, 4,   // float
      8, 8,   // double
      8, 8,   // long double (same as double; no wider FP hardware)
      8, 8,   // pointer
      16, 32, 64, 64, // signed bits
      1,   // plain char signed (GCC: no __CHAR_UNSIGNED__)
      0,   // signed >> arithmetic
      1,   // aggregate_align (1)
      SIZE_MAX, // struct_return_max: never lowered by the front end; the backend
                // returns every struct and union through the address in $251
      0,        // struct_args_split
      NULL,     // immediate_args
      NULL,     // va_class: va_arg is a walk over 8-byte slots, no argument classes
      0,        // ldouble_mant_dig: long double is double, binary64
      1,        // hw_sqrt: fsqrt
      0,        // double_mant_dig: binary64
      0,        // no_loop_opt
      1,        // big-endian
      BITFIELD_PACKED,
      0,        // bitfield_access_bits
      0,        // bitfield_unit_per_field
      0,        // jump_tables
      0 },      // no_coroutines

    // BESM-6: 48-bit word-oriented machine.
    // sizeof() values are in 8-bit bytes (CHAR_BIT = 8).
    // One machine word = 6 bytes.  Every scalar type is a single word — there are
    // no two-word scalar types.  long long is the same as long; long double is the
    // same as double.  All alignments are 1 word = 6 bytes.
    // Signed int/long/long long are 41-bit (sign + 40 value bits) inside the word;
    // unsigned types use the full 48-bit storage width.
    // _Bool is a word too: see the Target comment — a byte-sized _Bool would mean
    // packed storage and fat byte pointers on this machine.
    { "besm6",
      6, 6,   // _Bool   (1 word)
      6, 6,   // short   (1 word)
      6, 6,   // int     (1 word)
      6, 6,   // long    (1 word)
      6, 6,   // long long  (1 word, same as long)
      6, 6,   // float   (1 word, BESM-6 native FP)
      6, 6,   // double  (1 word, same as float)
      6, 6,   // long double (1 word, same as double)
      6, 6,   // pointer (1 word, 15-bit word address)
      41, 41, 41, 41, // signed bits
      0,   // plain char unsigned
      1,   // signed >> logical (BESM-6 shift unit does no sign extension)
      6,   // aggregate_align (6)
      6,   // struct_return_max (1 word)
      1,   // struct_args_split
      besm6_immediate_args,
      NULL, // va_class
      0,    // ldouble_mant_dig
      0,    // hw_sqrt
      0,    // double_mant_dig
      1,    // no_loop_opt
      1,    // big-endian: chars are packed from the most significant end of the word,
            // and so are bit-fields
      .no_coroutines = 1 },

    // AArch64 on macOS: Apple's arm64 ABI.  As AArch64 but for a signed plain char and a
    // long double that is double; va_arg walks the stack, where every variadic argument is.
    { "aarch64-darwin",
      1, 1,   // _Bool
      2, 2,   // short
      4, 4,   // int
      8, 8,   // long (LP64)
      8, 8,   // long long
      4, 4,   // float
      8, 8,   // double
      8, 8,   // long double (double)
      8, 8,   // pointer
      16, 32, 64, 64, // signed bits
      1,   // plain char signed (Apple)
      0,   // signed >> arithmetic
      1,   // aggregate_align (1)
      SIZE_MAX, // struct_return_max: the backend returns a large result through x8
      0,        // struct_args_split
      NULL,     // immediate_args
      tac_apple64_class, // va_class: by reference or in 8-byte stack slots
      0,        // ldouble_mant_dig: long double is double, binary64
      1,        // hw_sqrt: fsqrt
      0,        // double_mant_dig: binary64
      0,        // no_loop_opt
      0,        // little-endian
      BITFIELD_SYSV,
      0,        // bitfield_access_bits
      0,        // bitfield_unit_per_field
      0,        // jump_tables
      0 },      // no_coroutines

    // WebAssembly, as clang's wasm32-unknown-unknown: ILP32 with a signed plain char and a
    // software binary128 long double.
    { "wasm32",
      1, 1,   // _Bool
      2, 2,   // short
      4, 4,   // int
      4, 4,   // long (ILP32: same size as int)
      8, 8,   // long long (i64)
      4, 4,   // float (f32)
      8, 8,   // double (f64)
     16,16,   // long double (IEEE 754 binary128, software)
      4, 4,   // pointer
      16, 32, 32, 64, // signed bits
      1,   // plain char signed (clang wasm32)
      0,   // signed >> arithmetic
      1,   // aggregate_align (1)
      SIZE_MAX, // struct_return_max: never lowered by the front end; the backend
                // passes a struct holding one scalar as that scalar, any other
                // result through a hidden pointer
      0,        // struct_args_split
      NULL,     // immediate_args
      tac_wasm32_class, // va_class: by value in the caller's buffer, or by reference
      0,        // ldouble_mant_dig: binary128
      1,        // hw_sqrt: f64.sqrt
      0,        // double_mant_dig: binary64
      0,        // no_loop_opt
      0,        // little-endian
      BITFIELD_SYSV,
      0,        // bitfield_access_bits
      1,        // bitfield_unit_per_field: clang's single-element rule counts bit-fields
      1,        // jump_tables: br_table
      0 },      // no_coroutines
};
// clang-format on

#define NUM_TARGETS ((int)(sizeof(targets) / sizeof(targets[0])))

// Index of the default target (x86_64).
#define DEFAULT_TARGET_INDEX 4

const Target *target_config = &targets[DEFAULT_TARGET_INDEX];

// Hosted targets whose data model is that of a bare-metal one; wasm32-braam is wasm32
// with Braam's process model.
static const struct {
    const char *name, *model;
    int braam;
} aliases[] = {
    { "x86_64-linux", "x86_64", 0 },
    { "aarch64-linux", "aarch64", 0 },
    { "riscv64-linux", "riscv64", 0 },
    { "wasm32-braam", "wasm32", 1 },
};

const Target *target_lookup(const char *name)
{
    static Target braam; // the model's descriptor with the braam bit
    int with_braam = 0;
    for (size_t i = 0; i < sizeof(aliases) / sizeof(aliases[0]); i++)
        if (strcmp(aliases[i].name, name) == 0) {
            name       = aliases[i].model;
            with_braam = aliases[i].braam;
        }
    for (int i = 0; i < NUM_TARGETS; i++) {
        if (strcmp(targets[i].name, name) == 0) {
            if (!with_braam)
                return &targets[i];
            braam       = targets[i];
            braam.braam = 1;
            return &braam;
        }
    }
    return NULL;
}

void target_list(void)
{
    for (int i = 0; i < NUM_TARGETS; i++) {
        fprintf(stderr, "  %s\n", targets[i].name);
    }
    for (size_t i = 0; i < sizeof(aliases) / sizeof(aliases[0]); i++)
        fprintf(stderr, "  %s\n", aliases[i].name);
}

int target_word_addressed(void)
{
    return target_config->aggregate_align > 1;
}

Float128 target_ld_round(Float128 q)
{
    int mant = target_config ? target_config->ldouble_mant_dig : 0;
    if (mant == 24)
        return f128_from_double(f128_to_float(q)); // IEEE single: its exponent range too
    return mant ? f128_round(q, mant) : q;
}

int target_double_is_single(void)
{
    return target_config && target_config->double_mant_dig == 24;
}

double target_double_round(double d)
{
    // cppcheck-suppress suspiciousFloatingPointCast ; rounding to single is the point
    return target_double_is_single() ? (double)(float)d : d;
}

double target_double_from_i64(int64_t v)
{
    return target_double_is_single() ? (double)(float)v : (double)v;
}

double target_double_from_u64(uint64_t v)
{
    return target_double_is_single() ? (double)(float)v : (double)v;
}

int64_t sign_narrow(uint64_t bits, int w)
{
    if (w <= 0 || w >= 64)
        return (int64_t)bits;
    uint64_t mask = ((uint64_t)1 << w) - 1;
    uint64_t sbit = (uint64_t)1 << (w - 1);
    uint64_t low  = bits & mask;
    return (int64_t)((low ^ sbit) - sbit);
}

uint64_t unsigned_narrow(uint64_t bits, int w)
{
    if (w <= 0 || w >= 64)
        return bits;
    return bits & (((uint64_t)1 << w) - 1);
}
