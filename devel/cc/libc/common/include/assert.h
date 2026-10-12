/*
 * <assert.h> — diagnostics (C11 §7.2).
 *
 * Re-includable: the definition of assert depends on NDEBUG at each inclusion.
 * The active form calls a runtime handler (TODO: __assert_fail), which prints a
 * diagnostic and aborts.
 */

#include <stdlib.h>

#undef assert

#ifdef NDEBUG
#define assert(ignore) ((void)0)
#else

/* TODO: provide __assert_fail in the runtime. */
_Noreturn void __assert_fail(const char *expr, const char *file, int line);

#define assert(expr) ((expr) ? (void)0 : __assert_fail(#expr, __FILE__, __LINE__))

#endif
