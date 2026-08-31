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

/* The 48-bit word needs 64 bits; there is no 32-bit build. */
typedef uint64_t value_t;
typedef int status_t;

#endif /* BESM6_TYPES_H */
