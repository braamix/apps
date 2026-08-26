// crc32.h — compute the CRC-32 of a data stream, Mark Adler, 1995.
//
// Upstream said "read this after zip.h" and left it at that; here it says so
// itself, because clang-format sorts the includes of every file that has one.
#pragma once

#include "zip.h"

ZCONST ulg near *get_crc_table(void);
ulg crc32(ulg crc, ZCONST uch *buf, extent len);

#define CRC_32_TAB crc_32_tab

#define CRC32UPD(c, crctab) (crctab[((int)(c)) & 0xff] ^ ((c) >> 8))
#define CRC32(c, b, crctab) (crctab[((int)(c) ^ (b)) & 0xff] ^ ((c) >> 8))
