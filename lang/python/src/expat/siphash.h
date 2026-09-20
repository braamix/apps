/* ==========================================================================
 * siphash.h - SipHash-2-4 in a single header file
 * --------------------------------------------------------------------------
 * Derived by William Ahern from the reference implementation[1] published[2]
 * by Jean-Philippe Aumasson and Daniel J. Berstein.
 * Minimal changes by Sebastian Pipping and Victor Stinner on top, see below.
 * Licensed under the CC0 Public Domain Dedication license.
 *
 * 1. https://www.131002.net/siphash/siphash24.c
 * 2. https://www.131002.net/siphash/
 *
 * SPDX-License-Identifier: CC0-1.0
 * --------------------------------------------------------------------------
 * HISTORY:
 *
 * 2020-10-03  (Sebastian Pipping)
 *   - Drop support for Visual Studio 9.0/2008 and earlier
 *
 * 2019-08-03  (Sebastian Pipping)
 *   - Mark part of sip24_valid as to be excluded from clang-format
 *   - Re-format code using clang-format 9
 *
 * 2018-07-08  (Anton Maklakov)
 *   - Add "fall through" markers for GCC's -Wimplicit-fallthrough
 *
 * 2017-11-03  (Sebastian Pipping)
 *   - Hide sip_tobin and sip_binof unless SIPHASH_TOBIN macro is defined
 *
 * 2017-07-25  (Vadim Zeitlin)
 *   - Fix use of SIPHASH_MAIN macro
 *
 * 2017-07-05  (Sebastian Pipping)
 *   - Use _SIP_ULL macro to not require a C++11 compiler if compiled as C++
 *   - Add const qualifiers at two places
 *   - Ensure <=80 characters line length (assuming tab width 4)
 *
 * 2017-06-23  (Victor Stinner)
 *   - Address Win64 compile warnings
 *
 * 2017-06-18  (Sebastian Pipping)
 *   - Clarify license note in the header
 *   - Address C89 issues:
 *     - Stop using inline keyword (and let compiler decide)
 *     - Replace _Bool by int
 *     - Turn macro siphash24 into a function
 *     - Address invalid conversion (void pointer) by explicit cast
 *   - Address lack of stdint.h for Visual Studio 2003 to 2008
 *   - Always expose sip24_valid (for self-tests)
 *
 * 2012-11-04 - Born.  (William Ahern)
 * --------------------------------------------------------------------------
 * USAGE:
 *
 * SipHash-2-4 takes as input two 64-bit words as the key, some number of
 * message bytes, and outputs a 64-bit word as the message digest. This
 * implementation employs two data structures: a struct sipkey for
 * representing the key, and a struct siphash for representing the hash
 * state.
 *
 * For converting a 16-byte unsigned char array to a key, use sip_tokey,
 * which requires a key object as a parameter.
 *
 * \tunsigned char secret[16];
 * \tstruct sipkey key;
 * \tsip_tokey(&key, secret);
 *
 * For hashing a message, use sip24_init, sip24_update and sip24_final.
 *
 * \tstruct siphash state;
 * \tconst void *msg;
 * \tusize len;
 * \tu64 hash;
 *
 * \tsip24_init(&state, &key);
 * \tsip24_update(&state, msg, len);
 * \thash = sip24_final(&state);
 * --------------------------------------------------------------------------
 * NOTES:
 *
 * o The compound-literal forms -- sip_keyof, sip_binof and siphash24 -- are
 *   gone with the C they were written in, and so is sip24_valid; what is
 *   left is the three-call interface expat uses.
 *
 * o Uppercase macros may evaluate parameters more than once. Lowercase
 *   macros should not exhibit any such side effects.
 * ==========================================================================
 */
#pragma once

#include "kernel/types.h"

#define SIP_ULL(high, low) ((((u64)high) << 32) | (low))

#define SIP_ROTL(x, b) (u64)(((x) << (b)) | ((x) >> (64 - (b))))

#define SIP_U32TO8_LE(p, v)   \
    (p)[0] = (u8)((v) >> 0);  \
    (p)[1] = (u8)((v) >> 8);  \
    (p)[2] = (u8)((v) >> 16); \
    (p)[3] = (u8)((v) >> 24);

#define SIP_U64TO8_LE(p, v)                  \
    SIP_U32TO8_LE((p) + 0, (u32)((v) >> 0)); \
    SIP_U32TO8_LE((p) + 4, (u32)((v) >> 32));

#define SIP_U8TO64_LE(p)                                                                           \
    (((u64)((p)[0]) << 0) | ((u64)((p)[1]) << 8) | ((u64)((p)[2]) << 16) | ((u64)((p)[3]) << 24) | \
     ((u64)((p)[4]) << 32) | ((u64)((p)[5]) << 40) | ((u64)((p)[6]) << 48) |                       \
     ((u64)((p)[7]) << 56))

#define SIPHASH_INITIALIZER { 0, 0, 0, 0, { 0 }, 0, 0 }

struct siphash {
    u64 v0, v1, v2, v3;

    unsigned char buf[8], *p;
    u64 c;
}; /* struct siphash */

#define SIP_KEYLEN 16

struct sipkey {
    u64 k[2];
}; /* struct sipkey */

static struct sipkey *sip_tokey(struct sipkey *key, const void *src)
{
    key->k[0] = SIP_U8TO64_LE((const unsigned char *)src);
    key->k[1] = SIP_U8TO64_LE((const unsigned char *)src + 8);
    return key;
} /* sip_tokey() */

static void sip_round(struct siphash *H, const int rounds)
{
    int i;

    for (i = 0; i < rounds; i++) {
        H->v0 += H->v1;
        H->v1 = SIP_ROTL(H->v1, 13);
        H->v1 ^= H->v0;
        H->v0 = SIP_ROTL(H->v0, 32);

        H->v2 += H->v3;
        H->v3 = SIP_ROTL(H->v3, 16);
        H->v3 ^= H->v2;

        H->v0 += H->v3;
        H->v3 = SIP_ROTL(H->v3, 21);
        H->v3 ^= H->v0;

        H->v2 += H->v1;
        H->v1 = SIP_ROTL(H->v1, 17);
        H->v1 ^= H->v2;
        H->v2 = SIP_ROTL(H->v2, 32);
    }
} /* sip_round() */

static struct siphash *sip24_init(struct siphash *H, const struct sipkey *key)
{
    H->v0 = SIP_ULL(0x736f6d65U, 0x70736575U) ^ key->k[0];
    H->v1 = SIP_ULL(0x646f7261U, 0x6e646f6dU) ^ key->k[1];
    H->v2 = SIP_ULL(0x6c796765U, 0x6e657261U) ^ key->k[0];
    H->v3 = SIP_ULL(0x74656462U, 0x79746573U) ^ key->k[1];

    H->p = H->buf;
    H->c = 0;

    return H;
} /* sip24_init() */

#define sip_endof(a) (&(a)[sizeof(a) / sizeof *(a)])

static struct siphash *sip24_update(struct siphash *H, const void *src, usize len)
{
    const unsigned char *p = (const unsigned char *)src, *pe = p + len;
    u64 m;

    do {
        while (p < pe && H->p < sip_endof(H->buf))
            *H->p++ = *p++;

        if (H->p < sip_endof(H->buf))
            break;

        m = SIP_U8TO64_LE(H->buf);
        H->v3 ^= m;
        sip_round(H, 2);
        H->v0 ^= m;

        H->p = H->buf;
        H->c += 8;
    } while (p < pe);

    return H;
} /* sip24_update() */

static u64 sip24_final(struct siphash *H)
{
    const char left = (char)(H->p - H->buf);
    u64 b           = (H->c + left) << 56;

    switch (left) {
    case 7:
        b |= (u64)H->buf[6] << 48;
        [[fallthrough]];
    case 6:
        b |= (u64)H->buf[5] << 40;
        [[fallthrough]];
    case 5:
        b |= (u64)H->buf[4] << 32;
        [[fallthrough]];
    case 4:
        b |= (u64)H->buf[3] << 24;
        [[fallthrough]];
    case 3:
        b |= (u64)H->buf[2] << 16;
        [[fallthrough]];
    case 2:
        b |= (u64)H->buf[1] << 8;
        [[fallthrough]];
    case 1:
        b |= (u64)H->buf[0] << 0;
        [[fallthrough]];
    case 0:
        break;
    }

    H->v3 ^= b;
    sip_round(H, 2);
    H->v0 ^= b;
    H->v2 ^= 0xff;
    sip_round(H, 4);

    return H->v0 ^ H->v1 ^ H->v2 ^ H->v3;
} /* sip24_final() */
