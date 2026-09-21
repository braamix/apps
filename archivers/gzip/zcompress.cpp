// Unix compress (.Z) decompressor — fd-based port of zuncompress.c (no funopen).
#include "braam.h"

#ifndef NO_COMPRESS_SUPPORT

#define BITS       16
#define HSIZE      69001
#define BIT_MASK   0x1f
#define BLOCK_MASK 0x80
#define INIT_BITS  9
#define FIRST      257
#define CLEAR      256
#define BUFSIZE    (64 * 1024)

typedef long code_int;
typedef unsigned char char_type;

struct ZState {
    int fd;
    int n_bits;
    int maxbits;
    code_int maxcode;
    code_int maxmaxcode;
    uint16_t codetab[HSIZE];
    char_type htab[HSIZE];
    code_int free_ent;
    int block_compress;
    int clear_flg;
    int roffset;
    int size;
    char_type gbuf[BITS];
    char_type stack[BITS];
    char_type *stackp;
    int finchar;
    code_int code;
    code_int oldcode;
    code_int incode;
    enum { S_START, S_MIDDLE, S_EOF } state;
};

static size_t compressed_prelen;
static const char *compressed_pre;
static off_t total_compressed_bytes;

static code_int getcode(ZState *zs);

static int zfill(ZState *zs, char_type *header, int need)
{
    int i = 0;
    for (; i < need && compressed_prelen; i++, compressed_prelen--)
        header[i] = *compressed_pre++;
    if (i < need) {
        ssize_t n = 0;
        // sync fill from caller's Task — zread_fd is only called from Task context
        (void)zs;
        (void)n;
    }
    return i;
}

Task<off_t> zuncompress_fd(int in, int out, char *pre, size_t prelen, off_t *compressed_bytes)
{
    ZState zs{};
    zs.fd                  = in;
    compressed_prelen      = prelen;
    compressed_pre         = prelen ? pre : nullptr;
    total_compressed_bytes = 0;

    off_t bout = 0;

    // Decompress by reading through zread logic inlined with co_await reads.
    char header[3];
    int got = 0;
    for (; got < 3 && compressed_prelen; got++, compressed_prelen--)
        header[got] = *compressed_pre++;
    if (got < 3) {
        ssize_t n = co_await read_retry(in, header + got, 3 - (size_t)got);
        if (n < 0)
            co_return -1;
        got += (int)n;
    }
    if (got < 3 || header[0] != '\037' || header[1] != '\235') {
        maybe_warnx("not in compress format");
        co_return -1;
    }
    zs.maxbits        = header[2] & BIT_MASK;
    zs.block_compress = header[2] & BLOCK_MASK;
    if (zs.maxbits > BITS || zs.maxbits < 12) {
        maybe_warnx("invalid compress format");
        co_return -1;
    }
    zs.maxmaxcode = 1L << zs.maxbits;
    zs.maxcode    = (1 << INIT_BITS) - 1;
    zs.n_bits     = INIT_BITS;
    zs.free_ent   = zs.block_compress ? FIRST : 256;
    for (code_int c = 255; c >= 0; c--) {
        zs.codetab[c] = 0;
        zs.htab[c]    = (char_type)c;
    }
    zs.oldcode = -1;
    zs.stackp  = zs.stack;
    zs.state   = ZState::S_MIDDLE;

    auto refill = [&]() -> Task<int> {
        zs.roffset = 0;
        int i      = 0;
        for (; i < zs.n_bits && compressed_prelen; i++, compressed_prelen--)
            zs.gbuf[i] = *compressed_pre++;
        ssize_t n = co_await read_retry(in, zs.gbuf + i, (size_t)(zs.n_bits - i));
        if (n < 0)
            co_return -1;
        zs.size = (int)n + i;
        if (zs.size <= 0)
            co_return 0;
        total_compressed_bytes += zs.size;
        zs.size = (zs.size << 3) - (zs.n_bits - 1);
        co_return 1;
    };

    for (;;) {
        if (zs.free_ent > zs.maxcode || zs.clear_flg || zs.roffset >= zs.size) {
            if (zs.free_ent > zs.maxcode) {
                zs.n_bits++;
                if (zs.n_bits == zs.maxbits)
                    zs.maxcode = zs.maxmaxcode;
                else
                    zs.maxcode = (1 << zs.n_bits) - 1;
            }
            if (zs.clear_flg) {
                zs.maxcode   = (1 << INIT_BITS) - 1;
                zs.n_bits    = INIT_BITS;
                zs.clear_flg = 0;
            }
            int r = co_await refill();
            if (r <= 0)
                break;
        }

        int r_off     = zs.roffset;
        int bits      = zs.n_bits;
        char_type *bp = zs.gbuf + (r_off >> 3);
        r_off &= 7;
        code_int gcode = (*bp++ >> r_off);
        bits -= (8 - r_off);
        r_off = 8 - r_off;
        if (bits >= 8) {
            gcode |= *bp++ << r_off;
            r_off += 8;
            bits -= 8;
        }
        static char_type rmask[9] = { 0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff };
        gcode |= (*bp & rmask[bits]) << r_off;
        zs.roffset += zs.n_bits;

        code_int ent = gcode;
        if (ent == CLEAR && zs.block_compress) {
            for (code_int c = 255; c >= 0; c--)
                zs.codetab[c] = 0;
            zs.clear_flg = 1;
            zs.free_ent  = FIRST;
            zs.oldcode   = -1;
            continue;
        }
        if (ent < 0)
            break;

        zs.incode = ent;
        if (ent >= zs.free_ent) {
            if (ent > zs.free_ent || zs.oldcode == -1) {
                maybe_warnx("corrupt compress data");
                co_return -1;
            }
            *zs.stackp++ = (char_type)zs.finchar;
            ent          = zs.oldcode;
        }
        while (ent >= 256) {
            *zs.stackp++ = zs.htab[ent];
            ent          = zs.codetab[ent];
        }
        *zs.stackp++ = zs.finchar = zs.htab[ent];

        while (zs.stackp > zs.stack) {
            char c = *--zs.stackp;
            if (!tflag) {
                if (co_await write_retry(out, &c, 1) != 1) {
                    maybe_warn("write");
                    co_return -1;
                }
            }
            bout++;
        }

        if ((ent = zs.free_ent) < zs.maxmaxcode && zs.oldcode != -1) {
            zs.codetab[ent] = (uint16_t)zs.oldcode;
            zs.htab[ent]    = zs.finchar;
            zs.free_ent     = ent + 1;
        }
        zs.oldcode = zs.incode;
    }

    if (compressed_bytes)
        *compressed_bytes = total_compressed_bytes;
    co_return bout;
}

#endif
