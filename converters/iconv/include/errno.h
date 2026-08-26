/* Citrus's whole error protocol is "return an errno value", and `ret = errno`
 * after a failed allocation appears twenty times over. The process is single
 * threaded, so one int is the whole of it. */
#ifndef _ERRNO_H_
#define _ERRNO_H_

extern "C" int errno;

#define EPERM        1
#define ENOENT       2
#define EINTR        4
#define EIO          5
#define E2BIG        7
#define EBADF        9
#define ENOMEM       12
#define EACCES       13
#define EEXIST       17
#define ENODEV       19
#define ENOTDIR      20
#define EISDIR       21
#define EINVAL       22
#define ENAMETOOLONG 63
#define ENOSYS       78
#define EFTYPE       79
#define EOPNOTSUPP   102
#define EAGAIN       35
#define EILSEQ       92
#define ERANGE       34

#endif
