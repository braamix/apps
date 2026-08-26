#ifndef _SYS_ENDIAN_H_
#define _SYS_ENDIAN_H_

#include <sys/types.h>

static inline uint16_t be16toh(uint16_t v)
{
    return __builtin_bswap16(v);
}
static inline uint32_t be32toh(uint32_t v)
{
    return __builtin_bswap32(v);
}
static inline uint64_t be64toh(uint64_t v)
{
    return __builtin_bswap64(v);
}
static inline uint16_t htobe16(uint16_t v)
{
    return __builtin_bswap16(v);
}
static inline uint32_t htobe32(uint32_t v)
{
    return __builtin_bswap32(v);
}
static inline uint16_t le16toh(uint16_t v)
{
    return v;
}
static inline uint32_t le32toh(uint32_t v)
{
    return v;
}
static inline uint16_t htole16(uint16_t v)
{
    return v;
}
static inline uint32_t htole32(uint32_t v)
{
    return v;
}

#endif
