// nan — a quiet NaN (C11 §7.12.11.2).  musl's is in libbraam_math.a, but vcc names
// a C function nan as nan.vcc (backend/wasm/emit.c), which only vcc can define.
#include <math.h>

double nan(const char *tag)
{
    (void)tag;
    return NAN;
}
