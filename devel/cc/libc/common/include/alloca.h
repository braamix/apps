/*
 * <alloca.h> — alloca(n): n bytes, aligned for any object, that live until the
 * calling function returns.  The backend expands __builtin_alloca in place, on the
 * stack, and the function's return gives the memory back.  Not in a coroutine
 * (docs/Standard_Include_Files.md).
 */
#ifndef _ALLOCA_H
#define _ALLOCA_H

#include <stddef.h>

void *__builtin_alloca(size_t size);

#define alloca(size) __builtin_alloca(size)

#endif /* _ALLOCA_H */
