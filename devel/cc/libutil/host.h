//
// C with coroutines on Braam, plain C on the host (Plan.md, decision 1). A call
// that waits is a coroutine on Braam and is awaited; on the host coro(Y) and
// await are nothing, and the same call is C's own.
//
#pragma once

#ifdef __braam__
#include <braam.h> // braam_call, and <coro.h>
#elif defined(__vcc_coroutines__)
#include <coro.h>
#else
#define coro(Y)
#define await

// Braam's park-once, so ^C reaches a long run; on the host there is nothing to do.
static inline int braam_yield(void)
{
    return 0;
}
#endif

#include <stddef.h>
#include <stdio.h>

//
// The whole of a file, in memory from malloc with a NUL after its *len bytes; NULL
// with errno set when it cannot be read.
//
coro(braam_call *) char *read_file(const char *path, size_t *len);

//
// Close a stream from open_memstream(). It never waits, so a plain function may
// close one, which on Braam fclose() would not allow.
//
int memstream_close(FILE *f);
