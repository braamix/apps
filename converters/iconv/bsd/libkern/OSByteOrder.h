/* The names Apple's branches use, over the same builtins. */
#ifndef _OSBYTEORDER_H_
#define _OSBYTEORDER_H_

#include <sys/types.h>

#define OSSwapInt16(v) __builtin_bswap16(v)
#define OSSwapInt32(v) __builtin_bswap32(v)
#define OSSwapInt64(v) __builtin_bswap64(v)

#define OSSwapBigToHostInt16(v) __builtin_bswap16(v)
#define OSSwapBigToHostInt32(v) __builtin_bswap32(v)
#define OSSwapBigToHostInt64(v) __builtin_bswap64(v)
#define OSSwapHostToBigInt16(v) __builtin_bswap16(v)
#define OSSwapHostToBigInt32(v) __builtin_bswap32(v)
#define OSSwapHostToBigInt64(v) __builtin_bswap64(v)

#define OSSwapLittleToHostInt16(v) (v)
#define OSSwapLittleToHostInt32(v) (v)
#define OSSwapLittleToHostInt64(v) (v)
#define OSSwapHostToLittleInt16(v) (v)
#define OSSwapHostToLittleInt32(v) (v)
#define OSSwapHostToLittleInt64(v) (v)

#endif
