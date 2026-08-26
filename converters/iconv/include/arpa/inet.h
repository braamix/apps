/* wasm is little-endian, so the four network-order conversions are a swap. */
#ifndef _ARPA_INET_H_
#define _ARPA_INET_H_

#include <sys/types.h>

static inline uint32_t htonl(uint32_t v)
{
    return __builtin_bswap32(v);
}
static inline uint16_t htons(uint16_t v)
{
    return __builtin_bswap16(v);
}
static inline uint32_t ntohl(uint32_t v)
{
    return __builtin_bswap32(v);
}
static inline uint16_t ntohs(uint16_t v)
{
    return __builtin_bswap16(v);
}

#endif
