/* Citrus asserts its own invariants 46 times over. A wasm trap is the whole
 * report; iconv_assert writes a line first so it is not a silent death. */
#ifndef _ASSERT_H_
#define _ASSERT_H_

extern "C" void iconv_assert_fail(const char *what, const char *file, int line);

#ifdef NDEBUG
#define assert(e) ((void)0)
#else
#define assert(e) ((e) ? (void)0 : iconv_assert_fail(#e, __FILE__, __LINE__))
#endif

#endif
