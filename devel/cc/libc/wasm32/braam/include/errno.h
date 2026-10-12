/*
 * <errno.h> for Braam: C's three, and Braam's errors (src/kernel/result.h), each its
 * Error number plus 32, which is what the descriptor calls leave in errno.
 */
#ifndef _ERRNO_H
#define _ERRNO_H

extern int errno;

#define EDOM   1 /* math argument out of domain */
#define ERANGE 2 /* result out of range */
#define EILSEQ 3 /* illegal byte sequence */

#define EINVAL    33 /* Invalid */
#define ENOMEM    34 /* NoMemory */
#define ENOENT    35 /* NotFound */
#define EEXIST    36 /* Exists */
#define ENOTDIR   37 /* NotDir */
#define EISDIR    38 /* IsDir */
#define EPERM     39 /* Perm */
#define EIO       40 /* Io */
#define ECANCELED 41 /* Cancelled */
#define EAGAIN    42 /* Again */
#define ENOTSUP   43 /* Unsupported */
#define EPIPE     44 /* Closed */
#define ENOTEMPTY 45 /* NotEmpty */
#define ELOOP     46 /* Loop */
#define EINTR     47 /* Intr */
#define EBUSY     48 /* Busy */

#endif /* _ERRNO_H */
