// `_md5`, `_sha1`, `_sha2`, `_sha3` and `_blake2`: the hashes CPython's
// hashlib.py falls back to when there is no OpenSSL, which is always here.
//
// CPython takes these from HACL*; the algorithms are the published ones --
// RFC 1321, FIPS 180-4, FIPS 202 and RFC 7693 -- and what is CPython's is the
// surface: which module holds which type, what each constructor takes, and
// the three getters every hash object answers.
#include "bigint.h"
#include "gc.h"
#include "kernel/fmt.h"
#include "method.h"
#include "module.h"
#include "ops.h"
#include "posix.h"
#include "type.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

enum class Algo : u8 {
    Md5,
    Sha1,
    Sha224,
    Sha256,
    Sha384,
    Sha512,
    Sha3_224,
    Sha3_256,
    Sha3_384,
    Sha3_512,
    Shake128,
    Shake256,
    Blake2b,
    Blake2s,
};

inline u32 rol32(u32 x, u32 n)
{
    return (x << n) | (x >> (32 - n));
}

inline u32 ror32(u32 x, u32 n)
{
    return (x >> n) | (x << (32 - n));
}

inline u64 rol64(u64 x, u32 n)
{
    return n ? (x << n) | (x >> (64 - n)) : x;
}

inline u64 ror64(u64 x, u32 n)
{
    return (x >> n) | (x << (64 - n));
}

inline u32 le32(const u8 *p)
{
    return u32(p[0]) | u32(p[1]) << 8 | u32(p[2]) << 16 | u32(p[3]) << 24;
}

inline u32 be32(const u8 *p)
{
    return u32(p[3]) | u32(p[2]) << 8 | u32(p[1]) << 16 | u32(p[0]) << 24;
}

inline u64 le64(const u8 *p)
{
    return u64(le32(p)) | u64(le32(p + 4)) << 32;
}

inline u64 be64(const u8 *p)
{
    return u64(be32(p)) << 32 | u64(be32(p + 4));
}

inline void put_le32(u8 *p, u32 v)
{
    for (u32 i = 0; i < 4; i++)
        p[i] = u8(v >> (8 * i));
}

inline void put_be32(u8 *p, u32 v)
{
    for (u32 i = 0; i < 4; i++)
        p[3 - i] = u8(v >> (8 * i));
}

inline void put_le64(u8 *p, u64 v)
{
    for (u32 i = 0; i < 8; i++)
        p[i] = u8(v >> (8 * i));
}

inline void put_be64(u8 *p, u64 v)
{
    for (u32 i = 0; i < 8; i++)
        p[7 - i] = u8(v >> (8 * i));
}

// ------------------------------------------------------------ MD5

constexpr u32 MD5_K[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
};

constexpr u8 MD5_R[64] = {
    7,  12, 17, 22, 7,  12, 17, 22, 7,  12, 17, 22, 7,  12, 17, 22, 5,  9,  14, 20, 5,  9,
    14, 20, 5,  9,  14, 20, 5,  9,  14, 20, 4,  11, 16, 23, 4,  11, 16, 23, 4,  11, 16, 23,
    4,  11, 16, 23, 6,  10, 15, 21, 6,  10, 15, 21, 6,  10, 15, 21, 6,  10, 15, 21,
};

void md5_block(u32 *h, const u8 *p)
{
    u32 w[16];
    for (u32 i = 0; i < 16; i++)
        w[i] = le32(p + 4 * i);
    u32 a = h[0], b = h[1], c = h[2], d = h[3];
    for (u32 i = 0; i < 64; i++) {
        u32 f, g;
        if (i < 16) {
            f = (b & c) | (~b & d);
            g = i;
        } else if (i < 32) {
            f = (d & b) | (~d & c);
            g = (5 * i + 1) & 15;
        } else if (i < 48) {
            f = b ^ c ^ d;
            g = (3 * i + 5) & 15;
        } else {
            f = c ^ (b | ~d);
            g = (7 * i) & 15;
        }
        u32 t = d;
        d     = c;
        c     = b;
        b     = b + rol32(a + f + MD5_K[i] + w[g], MD5_R[i]);
        a     = t;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
}

// ------------------------------------------------------------ SHA-1

void sha1_block(u32 *h, const u8 *p)
{
    u32 w[80];
    for (u32 i = 0; i < 16; i++)
        w[i] = be32(p + 4 * i);
    for (u32 i = 16; i < 80; i++)
        w[i] = rol32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    u32 a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (u32 i = 0; i < 80; i++) {
        u32 f, k;
        if (i < 20) {
            f = (b & c) | (~b & d);
            k = 0x5a827999;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ed9eba1;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8f1bbcdc;
        } else {
            f = b ^ c ^ d;
            k = 0xca62c1d6;
        }
        u32 t = rol32(a, 5) + f + e + k + w[i];
        e     = d;
        d     = c;
        c     = rol32(b, 30);
        b     = a;
        a     = t;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
}

// ---------------------------------------------------------- SHA-256

constexpr u32 SHA256_K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

constexpr u32 SHA224_IV[8] = { 0xc1059ed8, 0x367cd507, 0x3070dd17, 0xf70e5939,
                               0xffc00b31, 0x68581511, 0x64f98fa7, 0xbefa4fa4 };
constexpr u32 SHA256_IV[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                               0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };

void sha256_block(u32 *h, const u8 *p)
{
    u32 w[64];
    for (u32 i = 0; i < 16; i++)
        w[i] = be32(p + 4 * i);
    for (u32 i = 16; i < 64; i++) {
        u32 s1 = ror32(w[i - 2], 17) ^ ror32(w[i - 2], 19) ^ (w[i - 2] >> 10);
        u32 s0 = ror32(w[i - 15], 7) ^ ror32(w[i - 15], 18) ^ (w[i - 15] >> 3);
        w[i]   = s1 + w[i - 7] + s0 + w[i - 16];
    }
    u32 a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (u32 i = 0; i < 64; i++) {
        u32 t1 = hh + (ror32(e, 6) ^ ror32(e, 11) ^ ror32(e, 25)) + ((e & f) ^ (~e & g)) +
                 SHA256_K[i] + w[i];
        u32 t2 = (ror32(a, 2) ^ ror32(a, 13) ^ ror32(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        hh     = g;
        g      = f;
        f      = e;
        e      = d + t1;
        d      = c;
        c      = b;
        b      = a;
        a      = t1 + t2;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
    h[5] += f;
    h[6] += g;
    h[7] += hh;
}

// ---------------------------------------------------------- SHA-512

constexpr u64 SHA512_K[80] = {
    0x428a2f98d728ae22, 0x7137449123ef65cd, 0xb5c0fbcfec4d3b2f, 0xe9b5dba58189dbbc,
    0x3956c25bf348b538, 0x59f111f1b605d019, 0x923f82a4af194f9b, 0xab1c5ed5da6d8118,
    0xd807aa98a3030242, 0x12835b0145706fbe, 0x243185be4ee4b28c, 0x550c7dc3d5ffb4e2,
    0x72be5d74f27b896f, 0x80deb1fe3b1696b1, 0x9bdc06a725c71235, 0xc19bf174cf692694,
    0xe49b69c19ef14ad2, 0xefbe4786384f25e3, 0x0fc19dc68b8cd5b5, 0x240ca1cc77ac9c65,
    0x2de92c6f592b0275, 0x4a7484aa6ea6e483, 0x5cb0a9dcbd41fbd4, 0x76f988da831153b5,
    0x983e5152ee66dfab, 0xa831c66d2db43210, 0xb00327c898fb213f, 0xbf597fc7beef0ee4,
    0xc6e00bf33da88fc2, 0xd5a79147930aa725, 0x06ca6351e003826f, 0x142929670a0e6e70,
    0x27b70a8546d22ffc, 0x2e1b21385c26c926, 0x4d2c6dfc5ac42aed, 0x53380d139d95b3df,
    0x650a73548baf63de, 0x766a0abb3c77b2a8, 0x81c2c92e47edaee6, 0x92722c851482353b,
    0xa2bfe8a14cf10364, 0xa81a664bbc423001, 0xc24b8b70d0f89791, 0xc76c51a30654be30,
    0xd192e819d6ef5218, 0xd69906245565a910, 0xf40e35855771202a, 0x106aa07032bbd1b8,
    0x19a4c116b8d2d0c8, 0x1e376c085141ab53, 0x2748774cdf8eeb99, 0x34b0bcb5e19b48a8,
    0x391c0cb3c5c95a63, 0x4ed8aa4ae3418acb, 0x5b9cca4f7763e373, 0x682e6ff3d6b2b8a3,
    0x748f82ee5defb2fc, 0x78a5636f43172f60, 0x84c87814a1f0ab72, 0x8cc702081a6439ec,
    0x90befffa23631e28, 0xa4506cebde82bde9, 0xbef9a3f7b2c67915, 0xc67178f2e372532b,
    0xca273eceea26619c, 0xd186b8c721c0c207, 0xeada7dd6cde0eb1e, 0xf57d4f7fee6ed178,
    0x06f067aa72176fba, 0x0a637dc5a2c898a6, 0x113f9804bef90dae, 0x1b710b35131c471b,
    0x28db77f523047d84, 0x32caab7b40c72493, 0x3c9ebe0a15c9bebc, 0x431d67c49c100d4c,
    0x4cc5d4becb3e42b6, 0x597f299cfc657e2a, 0x5fcb6fab3ad6faec, 0x6c44198c4a475817,
};

constexpr u64 SHA384_IV[8] = { 0xcbbb9d5dc1059ed8, 0x629a292a367cd507, 0x9159015a3070dd17,
                               0x152fecd8f70e5939, 0x67332667ffc00b31, 0x8eb44a8768581511,
                               0xdb0c2e0d64f98fa7, 0x47b5481dbefa4fa4 };
constexpr u64 SHA512_IV[8] = { 0x6a09e667f3bcc908, 0xbb67ae8584caa73b, 0x3c6ef372fe94f82b,
                               0xa54ff53a5f1d36f1, 0x510e527fade682d1, 0x9b05688c2b3e6c1f,
                               0x1f83d9abfb41bd6b, 0x5be0cd19137e2179 };

void sha512_block(u64 *h, const u8 *p)
{
    u64 w[80];
    for (u32 i = 0; i < 16; i++)
        w[i] = be64(p + 8 * i);
    for (u32 i = 16; i < 80; i++) {
        u64 s1 = ror64(w[i - 2], 19) ^ ror64(w[i - 2], 61) ^ (w[i - 2] >> 6);
        u64 s0 = ror64(w[i - 15], 1) ^ ror64(w[i - 15], 8) ^ (w[i - 15] >> 7);
        w[i]   = s1 + w[i - 7] + s0 + w[i - 16];
    }
    u64 a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (u32 i = 0; i < 80; i++) {
        u64 t1 = hh + (ror64(e, 14) ^ ror64(e, 18) ^ ror64(e, 41)) + ((e & f) ^ (~e & g)) +
                 SHA512_K[i] + w[i];
        u64 t2 = (ror64(a, 28) ^ ror64(a, 34) ^ ror64(a, 39)) + ((a & b) ^ (a & c) ^ (b & c));
        hh     = g;
        g      = f;
        f      = e;
        e      = d + t1;
        d      = c;
        c      = b;
        b      = a;
        a      = t1 + t2;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
    h[5] += f;
    h[6] += g;
    h[7] += hh;
}

// ------------------------------------------------------------ Keccak

constexpr u64 KECCAK_RC[24] = {
    0x0000000000000001, 0x0000000000008082, 0x800000000000808a, 0x8000000080008000,
    0x000000000000808b, 0x0000000080000001, 0x8000000080008081, 0x8000000000008009,
    0x000000000000008a, 0x0000000000000088, 0x0000000080008009, 0x000000008000000a,
    0x000000008000808b, 0x800000000000008b, 0x8000000000008089, 0x8000000000008003,
    0x8000000000008002, 0x8000000000000080, 0x000000000000800a, 0x800000008000000a,
    0x8000000080008081, 0x8000000000008080, 0x0000000080000001, 0x8000000080008008,
};

constexpr u8 KECCAK_ROT[25] = {
    0, 1, 62, 28, 27, 36, 44, 6, 55, 20, 3, 10, 43, 25, 39, 41, 45, 15, 21, 8, 18, 2, 61, 56, 14,
};

__attribute__((noinline)) void keccak_f(u64 *s)
{
    for (u32 round = 0; round < 24; round++) {
        u64 c[5], b[25];
        for (u32 x = 0; x < 5; x++)
            c[x] = s[x] ^ s[x + 5] ^ s[x + 10] ^ s[x + 15] ^ s[x + 20];
        for (u32 x = 0; x < 5; x++) {
            u64 d = c[(x + 4) % 5] ^ rol64(c[(x + 1) % 5], 1);
            for (u32 y = 0; y < 25; y += 5)
                s[y + x] ^= d;
        }
        for (u32 x = 0; x < 5; x++)
            for (u32 y = 0; y < 5; y++)
                b[y + 5 * ((2 * x + 3 * y) % 5)] = rol64(s[x + 5 * y], KECCAK_ROT[x + 5 * y]);
        for (u32 y = 0; y < 25; y += 5)
            for (u32 x = 0; x < 5; x++)
                s[y + x] = b[y + x] ^ (~b[y + (x + 1) % 5] & b[y + (x + 2) % 5]);
        s[0] ^= KECCAK_RC[round];
    }
}

// ----------------------------------------------------------- BLAKE2

constexpr u64 BLAKE2B_IV[8] = { 0x6a09e667f3bcc908, 0xbb67ae8584caa73b, 0x3c6ef372fe94f82b,
                                0xa54ff53a5f1d36f1, 0x510e527fade682d1, 0x9b05688c2b3e6c1f,
                                0x1f83d9abfb41bd6b, 0x5be0cd19137e2179 };
constexpr u32 BLAKE2S_IV[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };

constexpr u8 BLAKE2_SIGMA[12][16] = {
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
    { 14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3 },
    { 11, 8, 12, 0, 5, 2, 15, 13, 10, 14, 3, 6, 7, 1, 9, 4 },
    { 7, 9, 3, 1, 13, 12, 11, 14, 2, 6, 5, 10, 4, 0, 15, 8 },
    { 9, 0, 5, 7, 2, 4, 10, 15, 14, 1, 11, 12, 6, 8, 3, 13 },
    { 2, 12, 6, 10, 0, 11, 8, 3, 4, 13, 7, 5, 15, 14, 1, 9 },
    { 12, 5, 1, 15, 14, 13, 4, 10, 0, 7, 6, 3, 9, 2, 8, 11 },
    { 13, 11, 7, 14, 12, 1, 3, 9, 5, 0, 15, 4, 8, 6, 2, 10 },
    { 6, 15, 14, 9, 11, 3, 0, 8, 12, 2, 13, 7, 1, 4, 10, 5 },
    { 10, 2, 8, 4, 7, 6, 1, 5, 15, 11, 9, 14, 3, 12, 13, 0 },
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
    { 14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3 },
};

// t is the octet count including this block; f0 and f1 the finalization flags.
void blake2b_block(u64 *h, const u8 *p, u64 t, u64 f0, u64 f1)
{
    u64 m[16], v[16];
    for (u32 i = 0; i < 16; i++)
        m[i] = le64(p + 8 * i);
    for (u32 i = 0; i < 8; i++) {
        v[i]     = h[i];
        v[i + 8] = BLAKE2B_IV[i];
    }
    v[12] ^= t;
    v[14] ^= f0;
    v[15] ^= f1;
    auto G = [&](u32 a, u32 b, u32 c, u32 d, u64 x, u64 y) {
        v[a] = v[a] + v[b] + x;
        v[d] = ror64(v[d] ^ v[a], 32);
        v[c] = v[c] + v[d];
        v[b] = ror64(v[b] ^ v[c], 24);
        v[a] = v[a] + v[b] + y;
        v[d] = ror64(v[d] ^ v[a], 16);
        v[c] = v[c] + v[d];
        v[b] = ror64(v[b] ^ v[c], 63);
    };
    for (u32 r = 0; r < 12; r++) {
        const u8 *s = BLAKE2_SIGMA[r];
        G(0, 4, 8, 12, m[s[0]], m[s[1]]);
        G(1, 5, 9, 13, m[s[2]], m[s[3]]);
        G(2, 6, 10, 14, m[s[4]], m[s[5]]);
        G(3, 7, 11, 15, m[s[6]], m[s[7]]);
        G(0, 5, 10, 15, m[s[8]], m[s[9]]);
        G(1, 6, 11, 12, m[s[10]], m[s[11]]);
        G(2, 7, 8, 13, m[s[12]], m[s[13]]);
        G(3, 4, 9, 14, m[s[14]], m[s[15]]);
    }
    for (u32 i = 0; i < 8; i++)
        h[i] ^= v[i] ^ v[i + 8];
}

void blake2s_block(u32 *h, const u8 *p, u64 t, u32 f0, u32 f1)
{
    u32 m[16], v[16];
    for (u32 i = 0; i < 16; i++)
        m[i] = le32(p + 4 * i);
    for (u32 i = 0; i < 8; i++) {
        v[i]     = h[i];
        v[i + 8] = BLAKE2S_IV[i];
    }
    v[12] ^= u32(t);
    v[13] ^= u32(t >> 32);
    v[14] ^= f0;
    v[15] ^= f1;
    auto G = [&](u32 a, u32 b, u32 c, u32 d, u32 x, u32 y) {
        v[a] = v[a] + v[b] + x;
        v[d] = ror32(v[d] ^ v[a], 16);
        v[c] = v[c] + v[d];
        v[b] = ror32(v[b] ^ v[c], 12);
        v[a] = v[a] + v[b] + y;
        v[d] = ror32(v[d] ^ v[a], 8);
        v[c] = v[c] + v[d];
        v[b] = ror32(v[b] ^ v[c], 7);
    };
    for (u32 r = 0; r < 10; r++) {
        const u8 *s = BLAKE2_SIGMA[r];
        G(0, 4, 8, 12, m[s[0]], m[s[1]]);
        G(1, 5, 9, 13, m[s[2]], m[s[3]]);
        G(2, 6, 10, 14, m[s[4]], m[s[5]]);
        G(3, 7, 11, 15, m[s[6]], m[s[7]]);
        G(0, 5, 10, 15, m[s[8]], m[s[9]]);
        G(1, 6, 11, 12, m[s[10]], m[s[11]]);
        G(2, 7, 8, 13, m[s[12]], m[s[13]]);
        G(3, 4, 9, 14, m[s[14]], m[s[15]]);
    }
    for (u32 i = 0; i < 8; i++)
        h[i] ^= v[i] ^ v[i + 8];
}

// --------------------------------------------------------- the object

// One state for every algorithm: the widest of them sets the size.
struct HashObj : Obj {
    Algo algo;
    u8 digest;    // octets out; 0 for a SHAKE
    u8 block;     // octets per block, the rate for Keccak
    u8 last_node; // BLAKE2's
    u32 fill;     // octets waiting in buf
    u64 count;    // octets absorbed, BLAKE2's t among them
    u32 h32[8];
    u64 h64[25];
    u8 buf[200];
};

extern const Type *const HASH_TYPES[14];

HashObj *hash_of(Value v)
{
    return static_cast<HashObj *>(v.obj());
}

struct AlgoInfo {
    Str name;
    u8 digest;
    u8 block;
};

constexpr AlgoInfo INFO[] = {
    { "md5", 16, 64 },       { "sha1", 20, 64 },      { "sha224", 28, 64 },
    { "sha256", 32, 64 },    { "sha384", 48, 128 },   { "sha512", 64, 128 },
    { "sha3_224", 28, 144 }, { "sha3_256", 32, 136 }, { "sha3_384", 48, 104 },
    { "sha3_512", 64, 72 },  { "shake_128", 0, 168 }, { "shake_256", 0, 136 },
    { "blake2b", 64, 128 },  { "blake2s", 32, 64 },
};

bool is_keccak(Algo a)
{
    return a >= Algo::Sha3_224 && a <= Algo::Shake256;
}

bool is_shake(Algo a)
{
    return a == Algo::Shake128 || a == Algo::Shake256;
}

bool is_blake2(Algo a)
{
    return a == Algo::Blake2b || a == Algo::Blake2s;
}

HashObj *hash_alloc(Algo algo)
{
    Obj *o = obj_alloc(HASH_TYPES[u32(algo)], sizeof(HashObj));
    if (!o)
        return oom(), nullptr;
    HashObj *h = static_cast<HashObj *>(o);
    h->algo    = algo;
    h->digest  = INFO[u32(algo)].digest;
    h->block   = INFO[u32(algo)].block;
    h->fill = 0, h->count = 0, h->last_node = 0;
    for (u64 &w : h->h64)
        w = 0;
    for (u32 &w : h->h32)
        w = 0;
    switch (algo) {
    case Algo::Md5:
        h->h32[0] = 0x67452301, h->h32[1] = 0xefcdab89;
        h->h32[2] = 0x98badcfe, h->h32[3] = 0x10325476;
        break;
    case Algo::Sha1:
        h->h32[0] = 0x67452301, h->h32[1] = 0xefcdab89, h->h32[2] = 0x98badcfe;
        h->h32[3] = 0x10325476, h->h32[4] = 0xc3d2e1f0;
        break;
    case Algo::Sha224:
    case Algo::Sha256:
        for (u32 i = 0; i < 8; i++)
            h->h32[i] = algo == Algo::Sha224 ? SHA224_IV[i] : SHA256_IV[i];
        break;
    case Algo::Sha384:
    case Algo::Sha512:
        for (u32 i = 0; i < 8; i++)
            h->h64[i] = algo == Algo::Sha384 ? SHA384_IV[i] : SHA512_IV[i];
        break;
    default:
        break;
    }
    return h;
}

void compress(HashObj *h, const u8 *p)
{
    switch (h->algo) {
    case Algo::Md5:
        md5_block(h->h32, p);
        break;
    case Algo::Sha1:
        sha1_block(h->h32, p);
        break;
    case Algo::Sha224:
    case Algo::Sha256:
        sha256_block(h->h32, p);
        break;
    case Algo::Sha384:
    case Algo::Sha512:
        sha512_block(h->h64, p);
        break;
    default:
        for (u32 i = 0; i < h->block; i++)
            h->h64[i / 8] ^= u64(p[i]) << (8 * (i % 8));
        keccak_f(h->h64);
        break;
    }
}

__attribute__((noinline)) void hash_update(HashObj *h, const u8 *p, usize n)
{
    u32 bs = h->block;
    if (is_blake2(h->algo)) {
        // The last block is held back: only finalization may compress it.
        while (n) {
            if (h->fill == bs) {
                h->count += bs;
                if (h->algo == Algo::Blake2b)
                    blake2b_block(h->h64, h->buf, h->count, 0, 0);
                else
                    blake2s_block(h->h32, h->buf, h->count, 0, 0);
                h->fill = 0;
            }
            usize take = bs - h->fill < n ? bs - h->fill : n;
            for (usize i = 0; i < take; i++)
                h->buf[h->fill + i] = p[i];
            h->fill += u32(take);
            p += take;
            n -= take;
        }
        return;
    }
    if (!is_keccak(h->algo))
        h->count += n;
    if (h->fill) {
        while (n && h->fill < bs) {
            h->buf[h->fill++] = *p++;
            n--;
        }
        if (h->fill < bs)
            return;
        compress(h, h->buf);
        h->fill = 0;
    }
    for (; n >= bs; n -= bs, p += bs)
        compress(h, p);
    for (usize i = 0; i < n; i++)
        h->buf[i] = p[i];
    h->fill = u32(n);
}

// The digest of what has been absorbed, leaving the object as it was. `n` is
// the length a SHAKE is asked for; out has room for it.
__attribute__((noinline)) void hash_final(const HashObj *src, u8 *out, usize n)
{
    HashObj h = *src;
    switch (h.algo) {
    case Algo::Md5:
    case Algo::Sha1:
    case Algo::Sha224:
    case Algo::Sha256: {
        u64 bits = h.count * 8;
        u8 pad[72];
        u32 plen = (h.fill < 56 ? 56 : 120) - h.fill;
        pad[0]   = 0x80;
        for (u32 i = 1; i < plen; i++)
            pad[i] = 0;
        if (h.algo == Algo::Md5)
            put_le64(pad + plen, bits);
        else
            put_be64(pad + plen, bits);
        u64 keep = h.count;
        hash_update(&h, pad, plen + 8);
        h.count   = keep;
        u32 words = h.algo == Algo::Sha1 ? 5 : 8;
        u8 full[32];
        for (u32 i = 0; i < words && i < 8; i++) {
            if (h.algo == Algo::Md5)
                put_le32(full + 4 * i, h.h32[i]);
            else
                put_be32(full + 4 * i, h.h32[i]);
        }
        for (u32 i = 0; i < h.digest; i++)
            out[i] = full[i];
        return;
    }
    case Algo::Sha384:
    case Algo::Sha512: {
        u8 pad[144];
        u32 plen = (h.fill < 112 ? 112 : 240) - h.fill;
        pad[0]   = 0x80;
        for (u32 i = 1; i < plen; i++)
            pad[i] = 0;
        put_be64(pad + plen, h.count >> 61);
        put_be64(pad + plen + 8, h.count << 3);
        hash_update(&h, pad, plen + 16);
        u8 full[64];
        for (u32 i = 0; i < 8; i++)
            put_be64(full + 8 * i, h.h64[i]);
        for (u32 i = 0; i < h.digest; i++)
            out[i] = full[i];
        return;
    }
    case Algo::Blake2b:
    case Algo::Blake2s: {
        for (u32 i = h.fill; i < h.block; i++)
            h.buf[i] = 0;
        h.count += h.fill;
        u8 full[64];
        if (h.algo == Algo::Blake2b) {
            blake2b_block(h.h64, h.buf, h.count, ~u64(0), h.last_node ? ~u64(0) : 0);
            for (u32 i = 0; i < 8; i++)
                put_le64(full + 8 * i, h.h64[i]);
        } else {
            blake2s_block(h.h32, h.buf, h.count, ~u32(0), h.last_node ? ~u32(0) : 0);
            for (u32 i = 0; i < 8; i++)
                put_le32(full + 4 * i, h.h32[i]);
        }
        for (u32 i = 0; i < h.digest; i++)
            out[i] = full[i];
        return;
    }
    default: {
        // Keccak: the suffix, the final bit, then squeeze.
        u8 block[200];
        for (u32 i = 0; i < h.block; i++)
            block[i] = i < h.fill ? h.buf[i] : 0;
        block[h.fill] ^= is_shake(h.algo) ? 0x1f : 0x06;
        block[h.block - 1] ^= 0x80;
        compress(&h, block);
        usize want = is_shake(h.algo) ? n : h.digest;
        usize at   = 0;
        for (;;) {
            for (u32 i = 0; i < h.block && at < want; i++)
                out[at++] = u8(h.h64[i / 8] >> (8 * (i % 8)));
            if (at == want)
                break;
            keccak_f(h.h64);
        }
        return;
    }
    }
}

// ---------------------------------------------------------- the surface

// _Py_hashlib_get_buffer_view.
bool hash_buffer(Value v, Str &out)
{
    if (is_str(v))
        return err_set("TypeError", "Strings must be encoded before hashing") == R::Ok;
    if (!buffer_like(v, out))
        return err_set("TypeError", "object supporting the buffer API required") == R::Ok;
    return true;
}

// _Py_hashlib_data_argument: what to hash, Nil for nothing. `r` is set when
// a warning has to be made first.
bool data_argument(Value data, Value string, Value &out, bool &warn)
{
    warn = false;
    out  = Value();
    if (!data.is_nil() && string.is_nil()) {
        out = data;
    } else if (data.is_nil() && !string.is_nil()) {
        warn = true;
        out  = string;
    } else if (!data.is_nil()) {
        return err_set("TypeError",
                       "'data' and 'string' are mutually exclusive and support for 'string' "
                       "keyword parameter is slated for removal in a future version.") == R::Ok;
    }
    return true;
}

constexpr Str STRING_DEPRECATED =
    "the 'string' keyword parameter is deprecated since Python 3.15 and slated for removal in "
    "Python 3.19; use the 'data' keyword parameter or pass the data to hash as a positional "
    "argument instead";

// The object made, and the warning about `string` when it was used.
R answer(Value obj, bool warn, Value &out)
{
    if (warn)
        return warn_then("DeprecationWarning", STRING_DEPRECATED, 1, obj, out);
    out = obj;
    return R::Ok;
}

// md5(data=b'', *, usedforsecurity=True, string=None), and the others made
// the same way.
R make_simple(const CallArgs &a, Str who, Algo algo, Value &out)
{
    static const Str NAMES[] = { "data", "usedforsecurity", "string" };
    Value v[3];
    if (!fn_take(a, who, NAMES, 0, v))
        return R::Err;
    if (a.nargs > 1) {
        char tmp[24];
        Buf<128> b;
        b.put(who).put("() takes at most 1 positional argument (");
        b.put(int_text(tmp, sizeof tmp, i64(a.nargs))).put(" given)");
        return err_set("TypeError", b.str());
    }
    Value data;
    bool warn = false;
    if (!data_argument(v[0], v[2], data, warn))
        return R::Err;
    Str bytes;
    if (!data.is_nil() && !hash_buffer(data, bytes))
        return R::Err;
    HashObj *h = hash_alloc(algo);
    if (!h)
        return R::Err;
    hash_update(h, reinterpret_cast<const u8 *>(bytes.data()), bytes.size());
    return answer(obj_value(h), warn, out);
}

HashObj *self_hash(const CallArgs &a, Str who)
{
    Value s = a.nargs ? method_self(a.args[0]) : Value();
    for (const Type *t : HASH_TYPES)
        if (s.is_obj() && s.obj()->type == t)
            return hash_of(s);
    Buf<96> b;
    b.put(who).put("() requires a hash object");
    return err_set2("TypeError", b.str(), type_name(s)), nullptr;
}

R h_update(const CallArgs &a, Value &out)
{
    HashObj *h = self_hash(a, "update");
    Str bytes;
    if (!h || !meth_args(a, "update", 1, 1) || !hash_buffer(a.args[1], bytes))
        return R::Err;
    hash_update(h, reinterpret_cast<const u8 *>(bytes.data()), bytes.size());
    out = value_none();
    return R::Ok;
}

R h_copy(const CallArgs &a, Value &out)
{
    HashObj *h = self_hash(a, "copy");
    if (!h || !meth_args(a, "copy", 0, 0))
        return R::Err;
    Obj *o = obj_alloc(h->type, sizeof(HashObj));
    if (!o)
        return oom();
    h          = self_hash(a, "copy");
    HashObj *c = static_cast<HashObj *>(o);
    Obj head   = *c;
    *c         = *h;
    c->type    = head.type;
    c->next    = head.next;
    c->grey    = head.grey;
    c->flags   = head.flags;
    out        = obj_value(c);
    return R::Ok;
}

R h_digest(const CallArgs &a, Value &out)
{
    HashObj *h = self_hash(a, "digest");
    if (!h || !meth_args(a, "digest", 0, 0))
        return R::Err;
    u8 d[64];
    hash_final(h, d, 0);
    out = bytes_new(Str(reinterpret_cast<const char *>(d), h->digest));
    return out.is_nil() ? R::Err : R::Ok;
}

R h_hexdigest(const CallArgs &a, Value &out)
{
    HashObj *h = self_hash(a, "hexdigest");
    if (!h || !meth_args(a, "hexdigest", 0, 0))
        return R::Err;
    u8 d[64];
    hash_final(h, d, 0);
    String s;
    if (!hex_with_sep(Str(reinterpret_cast<const char *>(d), h->digest), Value(), 0, false, s))
        return R::Err;
    out = str_of_bytes(s.str());
    return out.is_nil() ? R::Err : R::Ok;
}

// A SHAKE's digest(length) and hexdigest(length).
R shake_out(const CallArgs &a, Str who, bool hex, Value &out)
{
    HashObj *h               = self_hash(a, who);
    static const Str NAMES[] = { "length" };
    Value v[1];
    if (!h || !meth_take(a, who, NAMES, 1, v))
        return R::Err;
    i64 n = 0;
    if (!as_index(v[0], n))
        return err_not_index(v[0]);
    if (n < 0)
        return err_set("ValueError", "length cannot be negative");
    if (n >= (1 << 29))
        return err_set("OverflowError", "digest length is too large");
    String d;
    if (!string_sized(d, usize(n)))
        return oom();
    h = self_hash(a, who);
    if (n)
        hash_final(h, reinterpret_cast<u8 *>(d.data()), usize(n));
    if (!hex) {
        out = bytes_new(d.str());
        return out.is_nil() ? R::Err : R::Ok;
    }
    String s;
    if (!hex_with_sep(d.str(), Value(), 0, false, s))
        return R::Err;
    out = str_of_bytes(s.str());
    return out.is_nil() ? R::Err : R::Ok;
}

R shake_digest(const CallArgs &a, Value &out)
{
    return shake_out(a, "digest", false, out);
}

R shake_hexdigest(const CallArgs &a, Value &out)
{
    return shake_out(a, "hexdigest", true, out);
}

R hash_getattr(Value v, StrObj *name, Value &out)
{
    HashObj *h = hash_of(v);
    Str n      = name->str();
    if (n == "name")
        out = str_new(INFO[u32(h->algo)].name);
    else if (n == "digest_size")
        out = Value::of_int(h->digest);
    else if (n == "block_size")
        out = Value::of_int(h->block);
    else if (is_keccak(h->algo) && n == "_capacity_bits")
        out = Value::of_int(1600 - h->block * 8);
    else if (is_keccak(h->algo) && n == "_rate_bits")
        out = Value::of_int(h->block * 8);
    else if (is_keccak(h->algo) && n == "_suffix")
        out = bytes_new(is_shake(h->algo) ? Str("\x1f", 1) : Str("\x06", 1));
    else
        return R::NotImpl;
    return out.is_nil() ? R::Err : R::Ok;
}

constexpr Method HASH_METHODS[] = {
    { "copy", h_copy },
    { "digest", h_digest },
    { "hexdigest", h_hexdigest },
    { "update", h_update },
};

constexpr Method SHAKE_METHODS[] = {
    { "copy", h_copy },
    { "digest", shake_digest },
    { "hexdigest", shake_hexdigest },
    { "update", h_update },
};

#define HASH_TYPE(var, tname) \
    constexpr Type var{ .name = tname, .getattr = hash_getattr, .final = true };

HASH_TYPE(md5_type, "_md5.md5")
HASH_TYPE(sha1_type, "_sha1.sha1")
HASH_TYPE(sha224_type, "_sha2.SHA224Type")
HASH_TYPE(sha256_type, "_sha2.SHA256Type")
HASH_TYPE(sha384_type, "_sha2.SHA384Type")
HASH_TYPE(sha512_type, "_sha2.SHA512Type")
HASH_TYPE(sha3_224_type, "_sha3.sha3_224")
HASH_TYPE(sha3_256_type, "_sha3.sha3_256")
HASH_TYPE(sha3_384_type, "_sha3.sha3_384")
HASH_TYPE(sha3_512_type, "_sha3.sha3_512")
HASH_TYPE(shake128_type, "_sha3.shake_128")
HASH_TYPE(shake256_type, "_sha3.shake_256")
HASH_TYPE(blake2b_type, "_blake2.blake2b")
HASH_TYPE(blake2s_type, "_blake2.blake2s")

#undef HASH_TYPE

const Type *const HASH_TYPES[14] = {
    &md5_type,      &sha1_type,     &sha224_type,   &sha256_type,   &sha384_type,
    &sha512_type,   &sha3_224_type, &sha3_256_type, &sha3_384_type, &sha3_512_type,
    &shake128_type, &shake256_type, &blake2b_type,  &blake2s_type,
};

R b_md5(const CallArgs &a, Value &out)
{
    return make_simple(a, "md5", Algo::Md5, out);
}

R b_sha1(const CallArgs &a, Value &out)
{
    return make_simple(a, "sha1", Algo::Sha1, out);
}

R b_sha224(const CallArgs &a, Value &out)
{
    return make_simple(a, "sha224", Algo::Sha224, out);
}

R b_sha256(const CallArgs &a, Value &out)
{
    return make_simple(a, "sha256", Algo::Sha256, out);
}

R b_sha384(const CallArgs &a, Value &out)
{
    return make_simple(a, "sha384", Algo::Sha384, out);
}

R b_sha512(const CallArgs &a, Value &out)
{
    return make_simple(a, "sha512", Algo::Sha512, out);
}

R b_sha3_224(const CallArgs &a, Value &out)
{
    return make_simple(a, "sha3_224", Algo::Sha3_224, out);
}

R b_sha3_256(const CallArgs &a, Value &out)
{
    return make_simple(a, "sha3_256", Algo::Sha3_256, out);
}

R b_sha3_384(const CallArgs &a, Value &out)
{
    return make_simple(a, "sha3_384", Algo::Sha3_384, out);
}

R b_sha3_512(const CallArgs &a, Value &out)
{
    return make_simple(a, "sha3_512", Algo::Sha3_512, out);
}

R b_shake128(const CallArgs &a, Value &out)
{
    return make_simple(a, "shake_128", Algo::Shake128, out);
}

R b_shake256(const CallArgs &a, Value &out)
{
    return make_simple(a, "shake_256", Algo::Shake256, out);
}

// An int argument of blake2's: C int, whatever the value.
bool int_arg(Value v, Str who, i64 dflt, i64 &out)
{
    out = dflt;
    if (v.is_nil())
        return true;
    if (is_float(v))
        return err_set("TypeError", "'float' object cannot be interpreted as an integer") == R::Ok;
    if (!as_index(v, out))
        return err_not_index(v) == R::Ok;
    (void)who;
    if (out < -2147483648ll || out > 2147483647ll) {
        return err_set("OverflowError", "Python int too large to convert to C int") == R::Ok;
    }
    return true;
}

// _PyLong_UnsignedLong_Converter and its long long twin: 64 bits on the
// CPython these messages come from.
bool ulong_arg(Value v, u64 &out)
{
    out = 0;
    if (v.is_nil())
        return true;
    if (is_float(v))
        return err_set("TypeError", "'float' object cannot be interpreted as an integer") == R::Ok;
    if (!is_intval(v))
        return err_not_index(v) == R::Ok;
    i64 n = 0;
    if (as_index(v, n)) {
        if (n < 0)
            return err_set("ValueError", "Cannot convert negative int") == R::Ok;
        out = u64(n);
        return true;
    }
    Root b{ big_of_value(v) };
    if (b.v.is_nil())
        return false;
    BigObj *bo = big_of(b.v);
    if (bo->neg)
        return err_set("ValueError", "Cannot convert negative int") == R::Ok;
    if (bo->len > 2)
        return err_set("OverflowError", "Python int too large to convert to C unsigned long") ==
               R::Ok;
    out = u64(bo->limbs()[0]) | (bo->len > 1 ? u64(bo->limbs()[1]) << 32 : 0);
    return true;
}

bool buf_arg(Value v, Str who, Str what, Str &out)
{
    out = Str();
    if (v.is_nil())
        return true;
    if (buffer_like(v, out))
        return true;
    Buf<160> b;
    b.put(who).put("() argument '").put(what).put("' must be a bytes-like object, not ");
    b.put(type_name(v));
    return err_set("TypeError", b.str()) == R::Ok;
}

R blake2_new(const CallArgs &a, Algo algo, Value &out)
{
    bool big                 = algo == Algo::Blake2b;
    Str who                  = big ? "blake2b" : "blake2s";
    u32 outb                 = big ? 64 : 32;
    u32 keyb                 = big ? 64 : 32;
    u32 saltb                = big ? 16 : 8;
    static const Str NAMES[] = { "data",
                                 "digest_size",
                                 "key",
                                 "salt",
                                 "person",
                                 "fanout",
                                 "depth",
                                 "leaf_size",
                                 "node_offset",
                                 "node_depth",
                                 "inner_size",
                                 "last_node",
                                 "usedforsecurity",
                                 "string" };
    Value v[14];
    if (!fn_take(a, who, NAMES, 0, v))
        return R::Err;
    if (a.nargs > 1) {
        char tmp[24];
        Buf<128> b;
        b.put(who).put("() takes at most 1 positional argument (");
        b.put(int_text(tmp, sizeof tmp, i64(a.nargs))).put(" given)");
        return err_set("TypeError", b.str());
    }
    Value data;
    bool warn = false;
    if (!data_argument(v[0], v[13], data, warn))
        return R::Err;
    i64 digest_size, fanout, depth, node_depth, inner_size;
    u64 leaf_size, node_offset;
    Str key, salt, person;
    if (!int_arg(v[1], who, outb, digest_size) || !buf_arg(v[2], who, "key", key) ||
        !buf_arg(v[3], who, "salt", salt) || !buf_arg(v[4], who, "person", person) ||
        !int_arg(v[5], who, 1, fanout) || !int_arg(v[6], who, 1, depth) ||
        !ulong_arg(v[7], leaf_size) || !ulong_arg(v[8], node_offset) ||
        !int_arg(v[9], who, 0, node_depth) || !int_arg(v[10], who, 0, inner_size))
        return R::Err;
    bool last_node = !v[11].is_nil() && py_truth(v[11]);

    char t1[24], t2[24];
    if (digest_size <= 0 || u64(digest_size) > outb) {
        Buf<128> b;
        b.put("digest_size for ").put(who).put(" must be between 1 and ");
        b.put(int_text(t1, sizeof t1, outb)).put(" bytes, got ");
        b.put(int_text(t2, sizeof t2, digest_size));
        return err_set("ValueError", b.str());
    }
    struct Len {
        Str name;
        Str v;
        u32 max;
    };
    const Len LENS[] = { { "key", key, keyb },
                         { "salt", salt, saltb },
                         { "person", person, saltb } };
    for (const Len &l : LENS)
        if (l.v.size() > l.max) {
            Buf<128> b;
            b.put("maximum ").put(l.name).put(" length is ").put(int_text(t1, sizeof t1, l.max));
            b.put(" bytes, got ").put(int_text(t2, sizeof t2, i64(l.v.size())));
            return err_set("ValueError", b.str());
        }
    struct Tree {
        Str name;
        i64 v;
        i64 lo, hi;
    };
    const Tree TREES[] = { { "fanout", fanout, 0, 255 },
                           { "depth", depth, 1, 255 },
                           { "node_depth", node_depth, 0, 255 },
                           { "inner_size", inner_size, 0, outb } };
    for (const Tree &t : TREES)
        if (t.v < t.lo || t.v > t.hi) {
            Buf<128> b;
            b.put("'").put(t.name).put("' must be between ").put(int_text(t1, sizeof t1, t.lo));
            b.put(" and ").put(int_text(t2, sizeof t2, t.hi));
            return err_set("ValueError", b.str());
        }
    if (leaf_size > 0xffffffffull)
        return err_set("OverflowError", "'leaf_size' is too large");
    if (!big && node_offset > 0xffffffffffffull)
        return err_set("OverflowError", "'node_offset' is too large");

    Str bytes;
    if (!data.is_nil() && !hash_buffer(data, bytes))
        return R::Err;
    HashObj *h = hash_alloc(algo);
    if (!h)
        return R::Err;
    h->digest    = u8(digest_size);
    h->last_node = last_node;
    // The parameter block, over the IV.
    u8 p[64] = {};
    p[0]     = u8(digest_size);
    p[1]     = u8(key.size());
    p[2]     = u8(fanout);
    p[3]     = u8(depth);
    put_le32(p + 4, u32(leaf_size));
    if (big) {
        put_le64(p + 8, node_offset);
        p[16] = u8(node_depth);
        p[17] = u8(inner_size);
        for (usize i = 0; i < salt.size(); i++)
            p[32 + i] = u8(salt[i]);
        for (usize i = 0; i < person.size(); i++)
            p[48 + i] = u8(person[i]);
        for (u32 i = 0; i < 8; i++)
            h->h64[i] = BLAKE2B_IV[i] ^ le64(p + 8 * i);
    } else {
        put_le32(p + 8, u32(node_offset));
        p[12] = u8(node_offset >> 32);
        p[13] = u8(node_offset >> 40);
        p[14] = u8(node_depth);
        p[15] = u8(inner_size);
        for (usize i = 0; i < salt.size(); i++)
            p[16 + i] = u8(salt[i]);
        for (usize i = 0; i < person.size(); i++)
            p[24 + i] = u8(person[i]);
        for (u32 i = 0; i < 8; i++)
            h->h32[i] = BLAKE2S_IV[i] ^ le32(p + 4 * i);
    }
    if (!key.empty()) {
        u8 block[128] = {};
        for (usize i = 0; i < key.size(); i++)
            block[i] = u8(key[i]);
        hash_update(h, block, h->block);
    }
    hash_update(h, reinterpret_cast<const u8 *>(bytes.data()), bytes.size());
    return answer(obj_value(h), warn, out);
}

R b_blake2b(const CallArgs &a, Value &out)
{
    return blake2_new(a, Algo::Blake2b, out);
}

R b_blake2s(const CallArgs &a, Value &out)
{
    return blake2_new(a, Algo::Blake2s, out);
}

bool methods_once()
{
    static bool done = false;
    if (done)
        return true;
    for (u32 i = 0; i < 14; i++)
        if (!method_install(HASH_TYPES[i], is_shake(Algo(i)) ? SHAKE_METHODS : HASH_METHODS, 4))
            return false;
    done = true;
    return true;
}

// The type under `name`, beside the module's functions.
bool put_type(DictObj *into, Str name, const Type *t)
{
    Root rd{ obj_value(into) };
    Root w{ type_wrap(t) };
    return !w.v.is_nil() && mod_put(static_cast<DictObj *>(rd.v.obj()), name, w.v);
}

bool common(DictObj *into)
{
    return methods_once() && mod_int(into, "_GIL_MINSIZE", 2048);
}

constexpr ModDef MD5_DEFS[]  = { { "md5", b_md5 } };
constexpr ModDef SHA1_DEFS[] = { { "sha1", b_sha1 } };
constexpr ModDef SHA2_DEFS[] = {
    { "sha224", b_sha224 },
    { "sha256", b_sha256 },
    { "sha384", b_sha384 },
    { "sha512", b_sha512 },
};

bool class_int(const Type *t, Str name, i64 v)
{
    Root w{ type_wrap(t) };
    if (w.v.is_nil())
        return false;
    return mod_int(static_cast<DictObj *>(type_obj(w.v)->dict.obj()), name, v);
}

} // namespace

bool md5_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    DictObj *d = static_cast<DictObj *>(rd.v.obj());
    return common(d) && mod_defs(d, MD5_DEFS) && put_type(d, "MD5Type", &md5_type);
}

bool sha1_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    DictObj *d = static_cast<DictObj *>(rd.v.obj());
    return common(d) && mod_defs(d, SHA1_DEFS) && put_type(d, "SHA1Type", &sha1_type);
}

bool sha2_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    DictObj *d = static_cast<DictObj *>(rd.v.obj());
    return common(d) && mod_defs(d, SHA2_DEFS) && put_type(d, "SHA224Type", &sha224_type) &&
           put_type(d, "SHA256Type", &sha256_type) && put_type(d, "SHA384Type", &sha384_type) &&
           put_type(d, "SHA512Type", &sha512_type);
}

bool sha3_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    DictObj *d = static_cast<DictObj *>(rd.v.obj());
    return common(d) && mod_type(d, &sha3_224_type, b_sha3_224) &&
           mod_type(d, &sha3_256_type, b_sha3_256) && mod_type(d, &sha3_384_type, b_sha3_384) &&
           mod_type(d, &sha3_512_type, b_sha3_512) && mod_type(d, &shake128_type, b_shake128) &&
           mod_type(d, &shake256_type, b_shake256) && mod_str(d, "implementation", "HACL");
}

bool blake2_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    DictObj *d = static_cast<DictObj *>(rd.v.obj());
    if (!common(d) || !mod_type(d, &blake2b_type, b_blake2b) ||
        !mod_type(d, &blake2s_type, b_blake2s))
        return false;
    struct Const {
        Str name;
        i64 b, s;
    };
    constexpr Const CONSTS[] = {
        { "SALT_SIZE", 16, 8 },
        { "PERSON_SIZE", 16, 8 },
        { "MAX_KEY_SIZE", 64, 32 },
        { "MAX_DIGEST_SIZE", 64, 32 },
    };
    for (const Const &c : CONSTS) {
        Buf<32> nb, ns;
        nb.put("BLAKE2B_").put(c.name);
        ns.put("BLAKE2S_").put(c.name);
        if (!class_int(&blake2b_type, c.name, c.b) || !class_int(&blake2s_type, c.name, c.s) ||
            !mod_int(static_cast<DictObj *>(rd.v.obj()), nb.str(), c.b) ||
            !mod_int(static_cast<DictObj *>(rd.v.obj()), ns.str(), c.s))
            return false;
    }
    return true;
}
