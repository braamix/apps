#include "xxh3.h"

// llvm/lib/Support/xxhash.cpp, the scalar path.

namespace {

constexpr u32 PRIME32_1 = 0x9E3779B1;
constexpr u32 PRIME32_2 = 0x85EBCA77;
constexpr u32 PRIME32_3 = 0xC2B2AE3D;
constexpr u64 PRIME64_1 = 11400714785074694791ULL;
constexpr u64 PRIME64_2 = 14029467366897019727ULL;
constexpr u64 PRIME64_3 = 1609587929392839161ULL;
constexpr u64 PRIME64_4 = 9650029242287828579ULL;
constexpr u64 PRIME64_5 = 2870177450012600261ULL;
constexpr u64 PRIME_MX1 = 0x165667919E3779F9;
constexpr u64 PRIME_MX2 = 0x9FB21C651E98DF25;

constexpr usize SECRET_SIZE = 192;
constexpr usize SECRET_MIN  = 136;
constexpr usize STRIPE      = 64;
constexpr usize CONSUME     = 8;
constexpr usize ACCS        = 8;

// clang-format off
constexpr u8 SECRET[SECRET_SIZE] = {
    0xb8, 0xfe, 0x6c, 0x39, 0x23, 0xa4, 0x4b, 0xbe, 0x7c, 0x01, 0x81, 0x2c, 0xf7, 0x21, 0xad, 0x1c,
    0xde, 0xd4, 0x6d, 0xe9, 0x83, 0x90, 0x97, 0xdb, 0x72, 0x40, 0xa4, 0xa4, 0xb7, 0xb3, 0x67, 0x1f,
    0xcb, 0x79, 0xe6, 0x4e, 0xcc, 0xc0, 0xe5, 0x78, 0x82, 0x5a, 0xd0, 0x7d, 0xcc, 0xff, 0x72, 0x21,
    0xb8, 0x08, 0x46, 0x74, 0xf7, 0x43, 0x24, 0x8e, 0xe0, 0x35, 0x90, 0xe6, 0x81, 0x3a, 0x26, 0x4c,
    0x3c, 0x28, 0x52, 0xbb, 0x91, 0xc3, 0x00, 0xcb, 0x88, 0xd0, 0x65, 0x8b, 0x1b, 0x53, 0x2e, 0xa3,
    0x71, 0x64, 0x48, 0x97, 0xa2, 0x0d, 0xf9, 0x4e, 0x38, 0x19, 0xef, 0x46, 0xa9, 0xde, 0xac, 0xd8,
    0xa8, 0xfa, 0x76, 0x3f, 0xe3, 0x9c, 0x34, 0x3f, 0xf9, 0xdc, 0xbb, 0xc7, 0xc7, 0x0b, 0x4f, 0x1d,
    0x8a, 0x51, 0xe0, 0x4b, 0xcd, 0xb4, 0x59, 0x31, 0xc8, 0x9f, 0x7e, 0xc9, 0xd9, 0x78, 0x73, 0x64,
    0xea, 0xc5, 0xac, 0x83, 0x34, 0xd3, 0xeb, 0xc3, 0xc5, 0x81, 0xa0, 0xff, 0xfa, 0x13, 0x63, 0xeb,
    0x17, 0x0d, 0xdd, 0x51, 0xb7, 0xf0, 0xda, 0x49, 0xd3, 0x16, 0x55, 0x26, 0x29, 0xd4, 0x68, 0x9e,
    0x2b, 0x16, 0xbe, 0x58, 0x7d, 0x47, 0xa1, 0xfc, 0x8f, 0xf8, 0xb8, 0xd1, 0x7a, 0xd0, 0x31, 0xce,
    0x45, 0xcb, 0x3a, 0x8f, 0x95, 0x16, 0x04, 0x28, 0xaf, 0xd7, 0xfb, 0xca, 0xbb, 0x4b, 0x40, 0x7e,
};
// clang-format on

u32 read32(const u8 *p)
{
    return u32(p[0]) | u32(p[1]) << 8 | u32(p[2]) << 16 | u32(p[3]) << 24;
}

u64 read64(const u8 *p)
{
    return u64(read32(p)) | u64(read32(p + 4)) << 32;
}

u64 rotl64(u64 x, u32 r)
{
    return (x << r) | (x >> (64 - r));
}

u32 bswap32(u32 x)
{
    return (x >> 24) | ((x >> 8) & 0xff00) | ((x << 8) & 0xff0000) | (x << 24);
}

u64 bswap64(u64 x)
{
    return u64(bswap32(u32(x))) << 32 | bswap32(u32(x >> 32));
}

// A 64 by 64 multiply folded to 64 bits, by halves: there is no __multi3,
// and an optimizer would see this as the multiply that calls it.
__attribute__((optnone, noinline)) u64 mul128_fold64(u64 lhs, u64 rhs)
{
    u64 lo_lo = (lhs & 0xFFFFFFFF) * (rhs & 0xFFFFFFFF);
    u64 hi_lo = (lhs >> 32) * (rhs & 0xFFFFFFFF);
    u64 lo_hi = (lhs & 0xFFFFFFFF) * (rhs >> 32);
    u64 hi_hi = (lhs >> 32) * (rhs >> 32);
    u64 cross = (lo_lo >> 32) + (hi_lo & 0xFFFFFFFF) + lo_hi;
    u64 upper = (hi_lo >> 32) + (cross >> 32) + hi_hi;
    u64 lower = (cross << 32) | (lo_lo & 0xFFFFFFFF);
    return upper ^ lower;
}

u64 xxh64_avalanche(u64 h)
{
    h ^= h >> 33;
    h *= PRIME64_2;
    h ^= h >> 29;
    h *= PRIME64_3;
    h ^= h >> 32;
    return h;
}

u64 avalanche(u64 h)
{
    h ^= h >> 37;
    h *= PRIME_MX1;
    h ^= h >> 32;
    return h;
}

u64 len_1to3(const u8 *in, usize len, const u8 *secret)
{
    u32 combined =
        (u32(in[0]) << 16) | (u32(in[len >> 1]) << 24) | u32(in[len - 1]) | (u32(len) << 8);
    u64 bitflip = u64(read32(secret) ^ read32(secret + 4));
    return xxh64_avalanche(u64(combined) ^ bitflip);
}

u64 len_4to8(const u8 *in, usize len, const u8 *secret)
{
    u32 input1 = read32(in);
    u32 input2 = read32(in + len - 4);
    u64 acc    = read64(secret + 8) ^ read64(secret + 16);
    acc ^= u64(input2) | (u64(input1) << 32);
    acc ^= rotl64(acc, 49) ^ rotl64(acc, 24);
    acc *= PRIME_MX2;
    acc ^= (acc >> 35) + u64(len);
    acc *= PRIME_MX2;
    return acc ^ (acc >> 28);
}

u64 len_9to16(const u8 *in, usize len, const u8 *secret)
{
    u64 lo = (read64(secret + 24) ^ read64(secret + 32)) ^ read64(in);
    u64 hi = (read64(secret + 40) ^ read64(secret + 48)) ^ read64(in + len - 8);
    return avalanche(u64(len) + bswap64(lo) + hi + mul128_fold64(lo, hi));
}

u64 mix16(const u8 *in, const u8 *secret)
{
    return mul128_fold64(read64(secret) ^ read64(in), read64(secret + 8) ^ read64(in + 8));
}

u64 len_17to128(const u8 *in, usize len, const u8 *secret)
{
    u64 acc = len * PRIME64_1;
    acc += mix16(in, secret);
    u64 end = mix16(in + len - 16, secret + 16);
    if (len > 32) {
        acc += mix16(in + 16, secret + 32);
        end += mix16(in + len - 32, secret + 48);
        if (len > 64) {
            acc += mix16(in + 32, secret + 64);
            end += mix16(in + len - 48, secret + 80);
            if (len > 96) {
                acc += mix16(in + 48, secret + 96);
                end += mix16(in + len - 64, secret + 112);
            }
        }
    }
    return avalanche(acc + end);
}

u64 len_129to240(const u8 *in, usize len, const u8 *secret)
{
    u64 acc    = u64(len) * PRIME64_1;
    u32 rounds = len / 16;
    for (u32 i = 0; i < 8; i++)
        acc += mix16(in + 16 * i, secret + 16 * i);
    acc = avalanche(acc);
    for (u32 i = 8; i < rounds; i++)
        acc += mix16(in + 16 * i, secret + 16 * (i - 8) + 3);
    acc += mix16(in + len - 16, secret + SECRET_MIN - 17);
    return avalanche(acc);
}

void accumulate_512(u64 *acc, const u8 *in, const u8 *secret)
{
    for (usize i = 0; i < ACCS; i++) {
        u64 data = read64(in + 8 * i);
        u64 key  = data ^ read64(secret + 8 * i);
        acc[i ^ 1] += data;
        acc[i] += u64(u32(key)) * (key >> 32);
    }
}

void scramble(u64 *acc, const u8 *secret)
{
    for (usize i = 0; i < ACCS; i++) {
        acc[i] ^= acc[i] >> 47;
        acc[i] ^= read64(secret + 8 * i);
        acc[i] *= PRIME32_1;
    }
}

u64 hash_long(const u8 *in, usize len)
{
    usize per_block = (SECRET_SIZE - STRIPE) / CONSUME;
    usize block_len = STRIPE * per_block;
    usize blocks    = (len - 1) / block_len;
    u64 acc[ACCS]   = { PRIME32_3, PRIME64_1, PRIME64_2, PRIME64_3,
                        PRIME64_4, PRIME32_2, PRIME64_5, PRIME32_1 };
    for (usize n = 0; n < blocks; n++) {
        for (usize s = 0; s < per_block; s++)
            accumulate_512(acc, in + n * block_len + s * STRIPE, SECRET + s * CONSUME);
        scramble(acc, SECRET + SECRET_SIZE - STRIPE);
    }
    usize stripes = (len - 1 - block_len * blocks) / STRIPE;
    for (usize s = 0; s < stripes; s++)
        accumulate_512(acc, in + blocks * block_len + s * STRIPE, SECRET + s * CONSUME);
    accumulate_512(acc, in + len - STRIPE, SECRET + SECRET_SIZE - STRIPE - 7);
    u64 result = u64(len) * PRIME64_1;
    for (usize i = 0; i < 4; i++)
        result += mul128_fold64(acc[2 * i] ^ read64(SECRET + 11 + 16 * i),
                                acc[2 * i + 1] ^ read64(SECRET + 11 + 16 * i + 8));
    return avalanche(result);
}

} // namespace

u64 xxh3_64(const u8 *in, usize len)
{
    if (len > 240)
        return hash_long(in, len);
    if (len > 128)
        return len_129to240(in, len, SECRET);
    if (len > 16)
        return len_17to128(in, len, SECRET);
    if (len > 8)
        return len_9to16(in, len, SECRET);
    if (len >= 4)
        return len_4to8(in, len, SECRET);
    if (len)
        return len_1to3(in, len, SECRET);
    return xxh64_avalanche(read64(SECRET + 56) ^ read64(SECRET + 64));
}

u32 lld_hash(Str s)
{
    // hash_value(StringRef) XORs in the execution seed of a release build.
    return u32(xxh3_64(reinterpret_cast<const u8 *>(s.data()), s.size()) ^ 0xff51afd7ed558ccdULL);
}

bool dense_map_order(Span<const u32> hashes, Vec<u32> &order)
{
    Vec<u32> table; // bucket -> position + 1; 0 for empty
    u32 entries = 0;
    auto place  = [&](u32 pos) {
        u32 mask = table.size() - 1;
        u32 b    = hashes[pos] & mask;
        while (table[b])
            b = (b + 1) & mask;
        table[b] = pos + 1;
    };
    auto grow = [&](u32 buckets) {
        Vec<u32> old = move(table);
        if (!table.resize(buckets))
            return false;
        for (u32 &b : table)
            b = 0;
        for (u32 b : old)
            if (b)
                place(b - 1);
        return true;
    };
    for (u32 pos = 0; pos < hashes.size(); pos++) {
        if ((entries + 1) * 4 >= table.size() * 3 &&
            !grow(table.size() * 2 > 64 ? table.size() * 2 : 64))
            return false;
        place(pos);
        entries++;
    }
    order.clear();
    for (u32 b : table)
        if (b && !order.push(b - 1))
            return false;
    return true;
}
