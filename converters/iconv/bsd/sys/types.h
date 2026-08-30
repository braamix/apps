/* The integer names citrus spells the BSD way. Sizes come from the kernel's
 * own types.h, so u_int32_t and uint32_t are the same 32 bits it uses. */
#ifndef _SYS_TYPES_H_
#define _SYS_TYPES_H_

#include "kernel/types.h"

typedef unsigned char __uint8_t;
typedef unsigned short __uint16_t;
typedef unsigned int __uint32_t;
typedef unsigned long long __uint64_t;
typedef signed char __int8_t;
typedef short __int16_t;
typedef int __int32_t;
typedef long long __int64_t;

typedef unsigned char u_int8_t;
typedef unsigned short u_int16_t;
typedef unsigned int u_int32_t;
typedef unsigned long long u_int64_t;
typedef unsigned int u_int;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef signed char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef long long int64_t;

#ifndef NULL
#define NULL nullptr
#endif

typedef usize size_t;
typedef isize ssize_t;

#endif
