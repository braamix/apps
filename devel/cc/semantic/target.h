#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "float128.h"

//
// An intrinsic whose FIRST argument is an immediate field of the instruction word rather
// than a value: it must be a compile-time constant in [lo, hi].  The semantic pass folds
// it to a literal, so the back end can encode it.
//
typedef struct {
    const char *name;  // the intrinsic, e.g. "__besm6_stop"
    const char *what;  // names the argument in the diagnostic
    const char *range; // completes "<name>: <what> <value> ..." when out of range
    long lo, hi;
} ImmediateArg;

//
// Target descriptor: size and alignment of primitive scalar types for one
// supported architecture.  All values are in bytes (C addressable units,
// i.e. in the same unit as sizeof() returns).  sizeof(char) == 1 always
// and is not stored here; schar/uchar are likewise always 1 byte.
// sizeof(enum) == sizeof(int) by convention.
//
// _Bool has a size of its own because C11 leaves its width implementation-defined
// and a byte is the wrong choice on a word-addressed machine: there the char sizes
// *mean* byte-packed storage and fat byte pointers, which would give `_Bool a[4]`
// six-per-word packing and `_Bool *p` a read-modify-write store, all to carry one
// bit.  BESM-6 therefore gives _Bool int's representation (one word); the
// byte-addressed targets keep the 1-byte _Bool their ABIs specify.
//
struct Tac_Type;

// How bit-fields are laid out (Target.bitfield_layout), as the reference compilers do.
typedef enum {
    // GCC's and clang's rule on most targets: a bit-field never straddles a boundary of
    // its declared type's alignment beyond the type's size, a named one aligns the struct
    // to its type, an unnamed one does not, and `:0` moves to the next such boundary.
    BITFIELD_SYSV,
    // AAPCS and AAPCS64 (ELF): the same, but an unnamed bit-field, `:0` included, also
    // aligns the struct to its declared type.
    BITFIELD_AAPCS,
    // GCC without PCC_BITFIELD_TYPE_MATTERS (MMIX): bit-fields are packed back to back
    // whatever their type and leave the struct's alignment alone; `:0` moves to the next
    // 8-byte boundary and aligns the struct to 8 bytes.
    BITFIELD_PACKED,
} BitfieldLayout;

typedef struct {
    const char *name;
    size_t bool_size, bool_align;
    size_t short_size, short_align;
    size_t int_size, int_align;
    size_t long_size, long_align;
    size_t llong_size, llong_align; // long long / unsigned long long
    size_t float_size, float_align;
    size_t double_size, double_align;
    size_t ldouble_size, ldouble_align; // long double
    size_t pointer_size, pointer_align;
    // Signed integer value width in bits (two's-complement, sign bit included).
    // Normally <type>_size*8, but BESM-6 signed ints are 41-bit inside a 48-bit
    // word.  The unsigned value width is always the full storage <type>_size*8.
    int short_bits, int_bits, long_bits, llong_bits;
    // Signedness of plain `char` (target-defined in C): 1 = signed, 0 = unsigned.
    // `signed char` and `unsigned char` are unaffected (always signed/unsigned).
    int char_signed;
    // Right-shift of a *signed* operand: 0 = arithmetic (sign-preserving, the usual
    // C-implementation choice), 1 = logical (zero-fill).  BESM-6's shift unit does no
    // sign extension, so its backend lowers signed `>>` to a logical shift; the constant
    // folder consults this flag to fold the same value the backend would emit.
    int right_shift_is_logical;
    // Minimum alignment (bytes) of any struct/union.  On a word-addressed target the
    // smallest addressable/copyable unit is a machine word, so aggregates are padded up
    // to a whole word (BESM-6 = 6); otherwise this is 1 (natural C packing).  This keeps
    // array element strides a word multiple, so &arr[i] never lands mid-word.
    size_t aggregate_align;
    // Struct/union by value.  One returned that is wider than struct_return_max bytes
    // goes through a hidden pointer to a caller-allocated slot, passed as the first
    // argument (0 = every struct and union, whatever its size, as on MSP430; SIZE_MAX =
    // never, the backend returns every struct itself).  With
    // struct_args_split, an argument wider than a word is passed as that many word
    // arguments (BESM-6); otherwise it is passed whole and the backend applies its ABI.
    size_t struct_return_max;
    int struct_args_split;
    // Intrinsics with an immediate first argument, terminated by a NULL name; NULL if none.
    const ImmediateArg *immediate_args;
    // The value of __builtin_va_class(T): the ABI's argument class of T, which va_arg
    // hands to the runtime; NULL on a target without one (the builtin is rejected).
    int (*va_class)(const struct Tac_Type *t);
    // The long double significand in bits, when it is neither binary128 nor binary64:
    // 64 for the x87 extended format, 24 for IEEE single (AVR).  0 = binary128 when
    // long double is wider than double, else double's.
    int ldouble_mant_dig;
    // Square root is an instruction (an IEEE one, correctly rounded): the translator
    // lowers a call of the C library's sqrt to TAC sqrt_double instead of a call.
    int hw_sqrt;
    // The double significand in bits: 24 where double is IEEE single (AVR), 0 for the
    // host's binary64.  The constant folders compute in the host's double and round each
    // result to it.
    int double_mant_dig;
    // The loop optimizations (rotation, invariant code motion, induction variables) are
    // off: BESM-6 keeps the code it had before them.
    int no_loop_opt;
    // The most significant byte of a word comes first in memory (MMIX); bit-fields are
    // then allocated from the most significant bit of their storage unit.
    int big_endian;
    BitfieldLayout bitfield_layout;
    // A struct's TAC type lists clang's access units of its bit-fields rather than ours
    // (AVR, whose ABI passes a structure flattened into them): a run of bit-fields up to
    // another member or `:0`, split before a field on a byte boundary when the unit would
    // grow past this many bits.  0 = our storage units.
    int bitfield_access_bits;
    // A struct's TAC type lists a bit-field's storage unit once per bit-field it holds,
    // named or not, rather than once per place of a named one, and marks each `:0` by an
    // unnamed array of no bytes (RISC-V, whose FP calling convention, as clang has it,
    // counts every bit-field as a field of its own, and a `:0` ahead of the second).
    int bitfield_unit_per_field;
    // The backend takes TAC JUMP_TABLE (wasm32's br_table): a coroutine's dispatch on its
    // state is then one from three suspension points, else a chain of compares.
    int jump_tables;
    // Coroutines (_Coro, _Yield, _Await and the co_* operations) are not implemented:
    // the lowering is target-neutral, but BESM-6 has no runtime for them.
    int no_coroutines;
    // Braam's process model (wasm32-braam, docs/Braam.md §7): main is a coroutine
    // the runtime awaits.  Set on the alias, not in the table.
    int braam;
} Target;

// Active target.  Defaults to x86_64.  Set this before calling any
// pipeline function (typecheck_decl, translate, etc.).
extern const Target *target_config;

// Look up a target by name (e.g. "x86_64", "besm6").
// Returns NULL if the name is not recognised.
const Target *target_lookup(const char *name);

// Print all known target names to stderr, one per line.
void target_list(void);

// True when the smallest addressable unit is a machine word wider than a byte
// (BESM-6): a scalar char then occupies a whole word, its value in the low byte.
int target_word_addressed(void);

// A long double constant, carried in binary128, rounded to the target's long double
// significand (ldouble_mant_dig); unchanged where that is binary128 or double, which
// the constant folders handle themselves.
Float128 target_ld_round(Float128 q);

// A double constant, computed in the host's binary64, rounded to the target's double:
// binary32 where double_mant_dig is 24, else unchanged.  The rounding is exact for
// + - * / and sqrt (53 >= 2*24 + 2), so folding agrees with the target's arithmetic.
double target_double_round(double d);

// Whether the target's double is IEEE single (AVR): then float and double are one format.
int target_double_is_single(void);

// An integer converted to the target's double, rounded once: a 64-bit integer converted
// to binary64 first and then to binary32 could round twice.
double target_double_from_i64(int64_t v);
double target_double_from_u64(uint64_t v);

// Sign-extend the low `w` bits of `bits` to a 64-bit signed value (w in (0,64)).
// w<=0 or w>=64 means "no narrowing": return the full 64-bit pattern.  Both constant
// folders (semantic/typecheck.c and optimize/const_fold.c) wrap folded results to a
// target value width through these two helpers, so they narrow identically.
int64_t sign_narrow(uint64_t bits, int w);

// Mask `bits` to the low `w` bits (w in (0,64)).  w<=0 or w>=64 returns `bits`.
uint64_t unsigned_narrow(uint64_t bits, int w);

#ifdef __cplusplus
}
#endif
