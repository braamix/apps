/*
 * <stdarg.h> — variable arguments (C11 §7.16), wasm32 target.
 *
 * A variadic function takes one parameter after its named ones: the address of a buffer
 * the caller filled with the variable arguments, each in a slot of at least 4 bytes
 * aligned to its own type (a double at 8, a long double at 16), so va_list is a plain
 * pointer walking it (clang's).  va_start is __va_start(&ap), which the code generator
 * expands in place: it loads ap from that parameter.
 *
 * A structure holding a single scalar sits in its slot by value; any other structure or
 * union comes by reference, its slot holding the address of a copy.  The two cannot be
 * told apart by size, so va_arg asks __builtin_va_class(T): 1 for one by reference.  A
 * char or short comes promoted to int, a float to double.
 */
#ifndef _STDARG_H
#define _STDARG_H

typedef char *va_list;

void __va_start(va_list *ap);

#define va_start(ap, last) __va_start(&(ap))

#define __va_size(T)  ((sizeof(T) + 3) & ~3UL)
#define __va_align(T) (_Alignof(T) > 4 ? _Alignof(T) : 4)

#define va_arg(ap, T)                                                                             \
    (*(__builtin_va_class(T) ? *(T **)(((ap) += 4) - 4)                                           \
                             : (T *)(((ap) = (char *)(((unsigned long)(ap) + __va_align(T) - 1) & \
                                                      -(unsigned long)__va_align(T)) +            \
                                             __va_size(T)) -                                      \
                                     __va_size(T))))

#define va_end(ap) ((void)(ap))

#define va_copy(dest, src) ((dest) = (src))

#endif /* _STDARG_H */
