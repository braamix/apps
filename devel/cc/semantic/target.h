#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//
// Target descriptor: size and alignment of primitive scalar types for one
// supported architecture.  All values are in bytes (C addressable units,
// i.e. in the same unit as sizeof() returns).  sizeof(char) == 1 always
// and is not stored here; schar/uchar are likewise always 1 byte.
// sizeof(enum) == sizeof(int) by convention.
//
struct Tac_Type;

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
    // <type>_size*8.  The unsigned value width is always the full storage <type>_size*8.
    int short_bits, int_bits, long_bits, llong_bits;
    // Signedness of plain `char` (target-defined in C): 1 = signed, 0 = unsigned.
    // `signed char` and `unsigned char` are unaffected (always signed/unsigned).
    int char_signed;
    // Struct/union by value.  One returned that is wider than struct_return_max bytes
    // goes through a hidden pointer to a caller-allocated slot, passed as the first
    // argument (SIZE_MAX = never, the backend returns every struct itself).
    size_t struct_return_max;
    // The value of __builtin_va_class(T): the ABI's argument class of T, which va_arg
    // hands to the runtime; NULL on a target without one (the builtin is rejected).
    int (*va_class)(const struct Tac_Type *t);
    // Braam's process model (wasm32-braam, docs/Braam.md §7): main is a coroutine
    // the runtime awaits.  Set on the alias, not in the table.
    int braam;
} Target;

// Active target.  Defaults to wasm32.  Set this before calling any
// pipeline function (typecheck_decl, translate, etc.).
extern const Target *target_config;

// Look up a target by name (e.g. "wasm32", "wasm32-braam").
// Returns NULL if the name is not recognised.
const Target *target_lookup(const char *name);

// Print all known target names to stderr, one per line.
void target_list(void);

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
