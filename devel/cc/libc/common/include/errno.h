/*
 * <errno.h> — errors (C11 §7.5).
 *
 * TODO: provide the errno object in the runtime.  The error numbers below are a
 * small, non-POSIX set sufficient for the C library.
 */
#ifndef _ERRNO_H
#define _ERRNO_H

extern int errno;

#define EDOM   1 /* math argument out of domain */
#define ERANGE 2 /* result out of range */
#define EILSEQ 3 /* illegal byte sequence */
#define EINVAL 4 /* invalid argument */
#define ENOMEM 5 /* out of memory */
#define EIO    6 /* input/output error */

#endif /* _ERRNO_H */
