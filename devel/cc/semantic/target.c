#include "target.h"

#include <stdio.h>
#include <string.h>

#include "tac.h"

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
      SIZE_MAX, // struct_return_max: never lowered by the front end; the backend
                // passes a struct holding one scalar as that scalar, any other
                // result through a hidden pointer
      tac_wasm32_class }, // va_class: by value in the caller's buffer, or by reference
};
// clang-format on

#define NUM_TARGETS ((int)(sizeof(targets) / sizeof(targets[0])))

// Index of the default target (wasm32).
#define DEFAULT_TARGET_INDEX 0

const Target *target_config = &targets[DEFAULT_TARGET_INDEX];

// wasm32-braam is wasm32 with Braam's process model.
static const struct {
    const char *name, *model;
    int braam;
} aliases[] = {
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
        diag_printf("  %s\n", targets[i].name);
    }
    for (size_t i = 0; i < sizeof(aliases) / sizeof(aliases[0]); i++)
        diag_printf("  %s\n", aliases[i].name);
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
