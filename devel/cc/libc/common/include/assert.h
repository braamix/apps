/*
 * <assert.h> — diagnostics (C11 §7.2).
 *
 * Re-includable: the definition of assert depends on NDEBUG at each inclusion.
 * The active form calls the runtime handler __assert_fail, which prints a
 * diagnostic and aborts.
 */

#include <stdlib.h>

#undef assert

#ifdef NDEBUG
#define assert(ignore) ((void)0)
#else

/* wasm32-braam's is in sys.c. */
_Noreturn void __assert_fail(const char *expr, const char *file, int line);

#define assert(expr) ((expr) ? (void)0 : __assert_fail(#expr, __FILE__, __LINE__))

#endif
