#!/usr/bin/env python3
"""Generate unlz_fd.cpp from FreeBSD unlz.c (memory input, fd output)."""

import re
from pathlib import Path

UP = Path("/Users/vak/Project/BSD/FreeBSD-github/usr.bin/gzip/unlz.c")
OUT = Path(__file__).resolve().parent.parent / "unlz_fd.cpp"

HEADER = """// Lzip decompressor — fd output, memory input (from FreeBSD unlz.c).
#include "braam.h"

#ifndef NO_LZ_SUPPORT

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

struct LzIn {
    const unsigned char *p;
    size_t n;
    size_t i;
};

static int
lz_in_getc(LzIn *in)
{
    if (in->i >= in->n)
        return EOF;
    return (int)in->p[in->i++];
}

"""

FOOTER = """

#define HDR_SIZE 6
#define MIN_DICTIONARY_SIZE (1 << 12)
#define MAX_DICTIONARY_SIZE (1 << 29)

static const char hdrmagic[] = { 'L', 'Z', 'I', 'P', 1 };

static unsigned
lz_get_dict_size(unsigned char c)
{
	unsigned dict_size = 1 << (c & 0x1f);
	dict_size -= (dict_size >> 2) * ( (c >> 5) & 0x7);
	if (dict_size < MIN_DICTIONARY_SIZE || dict_size > MAX_DICTIONARY_SIZE)
		return 0;
	return dict_size;
}

Task<off_t>
unlz(int fin, int fout, char *pre, size_t prelen, off_t *bytes_in)
{
    if (lz_crc[0] == 0)
        lz_crc_init();

    unsigned char *buf = NULL;
    size_t cap = prelen + 64;
    size_t len = prelen;
    buf = (unsigned char *)malloc(cap);
    if (!buf)
        maybe_err("malloc");
    if (prelen)
        memcpy(buf, pre, prelen);

    for (;;) {
        if (len >= cap) {
            cap *= 2;
            unsigned char *n = (unsigned char *)realloc(buf, cap);
            if (!n)
                maybe_err("realloc");
            buf = n;
        }
        ssize_t nr = co_await read_retry(fin, buf + len, cap - len);
        if (nr < 0) {
            free(buf);
            co_return -1;
        }
        if (nr == 0)
            break;
        len += (size_t)nr;
    }

    if (len < HDR_SIZE) {
        free(buf);
        co_return -1;
    }
    if (memcmp(buf, hdrmagic, sizeof(hdrmagic)) != 0) {
        free(buf);
        co_return -1;
    }
    unsigned dict_size = lz_get_dict_size(buf[5]);
    if (dict_size == 0) {
        free(buf);
        co_return -1;
    }

    struct lz_decoder lz;
    off_t rv = -1;
    LzIn input = { buf, len, 0 };
    if (lz_create_mem(&lz, &input, fout, dict_size) != 0) {
        free(buf);
        co_return -1;
    }
    if (!lz_decode_member(&lz)) {
        lz_destroy(&lz);
        free(buf);
        co_return -1;
    }
    uint8_t trailer[TRAILER_SIZE];
    for (size_t i = 0; i < nitems(trailer); i++) {
        int c = lz_in_getc(&input);
        if (c == EOF) {
            lz_destroy(&lz);
            free(buf);
            co_return -1;
        }
        trailer[i] = (uint8_t)c;
    }
    unsigned crc = 0;
    for (int i = 3; i >= 0; --i) {
        crc <<= 8;
        crc += trailer[i];
    }
    int64_t data_size = 0;
    for (int i = 11; i >= 4; --i) {
        data_size <<= 8;
        data_size += trailer[i];
    }
    if (crc != lz_get_crc(&lz) || data_size != lz_get_data_position(&lz)) {
        lz_destroy(&lz);
        free(buf);
        co_return -1;
    }
    rv = data_size;
    if (bytes_in) {
        off_t ins = 0;
        for (int i = 19; i >= 12; --i) {
            ins <<= 8;
            ins += trailer[i];
        }
        *bytes_in = ins;
    }
    if (!tflag && lz.outlen) {
        ssize_t w = co_await write_retry(fout, lz.outbuf, lz.outlen);
        if (w != (ssize_t)lz.outlen) {
            maybe_warn("write");
            rv = -1;
        }
    }
    lz_destroy(&lz);
    free(buf);
    co_return rv;
}

#endif
"""


def main():
    text = UP.read_text()
    # Drop upstream includes and unlz(); keep from lz_crc through lz_decode.
    start = text.index("#define LZ_STATES")
    end = text.index("static off_t\nlz_decode(")
    core = text[start:end]

    core = core.replace("FILE *fp;", "LzIn *in;")
    core = core.replace("lz_rd_create(struct lz_range_decoder *rd, FILE *fp)", "lz_rd_create(struct lz_range_decoder *rd, LzIn *in)")
    core = core.replace("rd->fp = fp;", "rd->in = in;")
    core = core.replace("getc(rd->fp)", "lz_in_getc(rd->in)")
    core = core.replace("ferror(rd->fp)", "(rd->in->i > rd->in->n)")
    core = core.replace(
        "struct lz_decoder {\n\tFILE *fin, *fout;",
        "struct lz_decoder {\n\tLzIn *fin;\n\tint fdout;\n\tunsigned char *outbuf;\n\tsize_t outlen;\n\tsize_t outcap;",
    )
    core = core.replace(
        "static int\nlz_create(struct lz_decoder *lz, int fin, int fdout, int dict_size)",
        "static int\nlz_create_mem(struct lz_decoder *lz, LzIn *input, int fdout, int dict_size)",
    )
    core = core.replace(
        "lz->fin = fdopen(dup(fin), \"r\");\n\tif (lz->fin == NULL)\n\t\tgoto out;\n\n\tlz->fout = fdopen(dup(fdout), \"w\");\n\tif (lz->fout == NULL)\n\t\tgoto out;",
        "lz->fin = input;\n\tlz->fdout = fdout;",
    )
    core = core.replace("if (lz_rd_create(&lz->rdec, lz->fin) == -1)", "if (lz_rd_create(&lz->rdec, lz->fin) == -1)")
    core = re.sub(
        r"static int\nlz_flush\(struct lz_decoder \*lz\)\s*\{[\s\S]*?\n\}",
        """static int
lz_flush(struct lz_decoder *lz)
{
	off_t offs = lz->pos - lz->spos;
	if (offs <= 0)
		return -1;

	size_t size = (size_t)offs;
	if (!tflag) {
		if (lz->outlen + size > lz->outcap) {
			size_t nc = lz->outcap ? lz->outcap * 2 : 65536;
			while (lz->outlen + size > nc)
				nc *= 2;
			unsigned char *b = (unsigned char *)realloc(lz->outbuf, nc);
			if (!b)
				return -1;
			lz->outbuf = b;
			lz->outcap = nc;
		}
		memcpy(lz->outbuf + lz->outlen, lz->obuf + lz->spos, size);
		lz->outlen += size;
	}

	lz->wrapped = lz->pos >= lz->dict_size;
	if (lz->wrapped) {
		lz->ppos += lz->pos;
		lz->pos = 0;
	}
	lz->spos = lz->pos;
	return 0;
}""",
        core,
        count=1,
    )
    core = core.replace("lz->obuf = malloc(dict_size);", "lz->obuf = (uint8_t *)malloc(dict_size);")
    core = core.replace("while (!feof(lz->fin) && !ferror(lz->fin))", "while (lz->fin->i < lz->fin->n)")
    core = core.replace(
        "if (lz->fin)\n\t\tfclose(lz->fin);\n\tif (lz->fout)\n\t\tfclose(lz->fout);",
        "free(lz->outbuf);",
    )

    out = HEADER + core + FOOTER
    OUT.write_text(out)
    print("Wrote", OUT)


if __name__ == "__main__":
    main()
