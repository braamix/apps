/*
 * <sys/types.h> for Braam: the types of the descriptor and stat calls.  Sizes and
 * offsets are 64 bits, as Braam's replies carry them.
 */
#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H

#include <stddef.h>

typedef int ssize_t;
typedef long long off_t;
typedef unsigned mode_t;
typedef unsigned long long ino_t;
typedef unsigned dev_t;
typedef unsigned nlink_t;
typedef unsigned uid_t;
typedef unsigned gid_t;
typedef int pid_t;
typedef int blksize_t;
typedef long long blkcnt_t;

#endif /* _SYS_TYPES_H */
