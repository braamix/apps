/*
 * The scalar types the machine is written in.
 *
 * Copyright (c) 1993-2022, Robert M Supnik and the Open SIMH contributors
 * Copyright (c) 2026, Serge Vakulenko
 *
 * Their own header so that every other one is self-contained: machine.h needs
 * image.h and image.h needs these, which would otherwise be a cycle and force
 * the includes into an order.
 */
#ifndef BESM6_TYPES_H
#define BESM6_TYPES_H

#include <stdint.h>

#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

typedef int32_t int32;
typedef uint8_t uint8;
typedef uint32_t uint32;

typedef int t_stat; /* status */
typedef int t_bool; /* boolean */

typedef signed long long t_int64;
typedef unsigned long long t_uint64;

/* The 48-bit word needs 64 bits; there is no 32-bit build. */
typedef t_uint64 t_value;

#endif /* BESM6_TYPES_H */
