/* The eight BSD spellings citrus uses. Everything else in the real header is
 * about compilers and platforms that are not this one. */
#ifndef _SYS_CDEFS_H_
#define _SYS_CDEFS_H_

#define __BEGIN_DECLS extern "C" {
#define __END_DECLS   }

#define __restrict __restrict__
#define __inline   inline
#define __packed   __attribute__((packed))
#define __unused   __attribute__((unused))

/* Cast away a const, which citrus does where a region is handed to a reader. */
#define __DECONST(type, var) ((type)(unsigned long)(const void *)(var))

/* ELF symbol versioning; there is one link and no compatibility symbols. */
#define __sym_compat(sym, impl, verid)

#endif
