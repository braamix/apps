// What upstream's build system writes, written down instead: the four knobs
// CPython builds libexpat with, and the handful of C library names the code
// reaches for that this system spells another way.
//
// Nothing here is conditional. There is one target, one byte order and one
// character type, so every #if that asked about the host is gone with the C.
#pragma once

#include "kernel/types.h"

// CPython's Modules/expat/expat_config.h, less the entropy switches: the
// hash salt comes from proc_random() here, so no source has to be detected.
#define XML_NS            1
#define XML_DTD           1
#define XML_GE            1
#define XML_CONTEXT_BYTES 1024
#define BYTEORDER         1234

// The limits <limits.h> and <stdint.h> would have given.
#define INT_MAX    0x7fffffff
#define UINT_MAX   0xffffffffu
#define LONG_MAX   0x7fffffffL
#define ULONG_MAX  0xfffffffful
#define USIZE_MAX  0xffffffffu
#define UINT64_MAX 0xffffffffffffffffull

// assert goes. Every one of upstream's is a statement about the parser's own
// invariants, checkable by reading; there is no stderr to print to and no
// abort worth taking a process down with.
#define assert(cond) ((void)0)

// There is no C library here, and a builtin over a runtime length becomes a
// call to a symbol nothing defines. These two are the whole of <string.h>
// that this parser reaches for and cannot have inlined.
// The volatile is load-bearing: without it the optimiser recognises the loop
// and emits a call to the strlen it is standing in for.
inline usize expat_strlen(const char *s)
{
    const volatile char *p = s;
    while (*p)
        p++;
    return usize(p - static_cast<const volatile char *>(s));
}

inline int expat_memcmp(const char *a, const char *b, usize n)
{
    const volatile char *x = a, *y = b;
    for (usize i = 0; i < n; i++) {
        unsigned char p = (unsigned char)x[i], q = (unsigned char)y[i];
        if (p != q)
            return p < q ? -1 : 1;
    }
    return 0;
}

inline int expat_strncmp(const char *a, const char *b, usize n)
{
    const volatile char *p = a, *q = b;
    for (usize i = 0; i < n; i++) {
        unsigned char x = (unsigned char)p[i], y = (unsigned char)q[i];
        if (x != y)
            return x < y ? -1 : 1;
        if (!x)
            break;
    }
    return 0;
}

// The billion-laughs and allocation accounting keep their limits and lose
// their reporting, so nothing here formats a float to a stream that does not
// exist. The debug levels stay as the constant zero the code compares with.
#define EXPAT_DEBUG_LEVEL 0
