/* Copyright (c) 1980 Regents of the University of California */
/* sccs id:	@(#)ex_temp.h	6.1 10/18/80  */
/*
 * The editor uses a temporary file for files being edited, in a structure
 * similar to that of ed.
 * Lines are represented in core by a pointer into the temporary file which
 * is packed into 16 bits (32 on VMUNIX).  All but the low bit index the temp
 * file; the last is used by global commands.
 *
 * The editor does not garbage collect the temporary file.  When a new
 * file is edited, the temporary file is rather discarded and a new one
 * created for the new file.  Garbage collection would be rather complicated
 * in ex because of the general undo, and in any case would require more
 * work when throwing lines away because marks would have be carefully
 * checked before reallocating temporary file space.  Said another way,
 * each time you create a new line in the temporary file you get a unique
 * number back, and this is a property used by marks.
 *
 * ---
 *
 * Ported to Braam. There is no temp file: the blocks live in one heap block
 * and the file is never touched. The packing is untouched because on the
 * VMUNIX arm it degenerates to identity — SHFT is 0 and OFFBTS is 10 against a
 * BUFSIZ of 1024, so a `line` handle already *is* a byte offset with bit 0
 * reserved for global's mark. That is what lets getline() and putline() stay
 * upstream's own source.
 *
 * What is gone with the file: the two-buffer LRU (blocks are contiguous now,
 * so there is nothing to choose between), blkio, tflush, the MAXDIRT sync, and
 * struct header, which existed only so expreserve could reconstruct a buffer
 * after a crash.
 *
 * Never-reuse is kept exactly, so the arena grows by total bytes ever written
 * rather than by file size. TX_MAX is the ceiling, and hitting it is upstream's
 * own "Tmp file too large".
 */
#pragma once

#define BLKMSK 077777
#define BNDRY  2
#define INCRMT 02000
#define LBTMSK 01776
#define NMBLKS 077770
#define OFFBTS 10
#define OFFMSK 01777
#define SHFT   0

EXTERN short nleft; /* Number usable chars left in output buffer */
EXTERN int tline;   /* Current temp file ptr */

/*
 * The arena. It doubles from TX_INIT and stops at TX_MAX; growth moves it,
 * which is safe precisely because no `line` is a pointer.
 */
#define TX_INIT (64 * 1024)
#define TX_MAX  (8 * 1024 * 1024)

EXTERN char *tx_arena;
EXTERN int tx_size;

/*
 * The line pointer array, which replaces sbrk's break. This one may *not*
 * move: dot, dol, addr1, addr2, unddol, truedol, undap1, undap2, unddel,
 * undadot, wdot, names[] and vUNDdot are all raw line * into it. So it is
 * taken once at startup and morelines() only ever bumps endcore against the
 * limit, which is what sbrk effectively did.
 */
#define LX_SLOTS 131072 /* 512 KiB of handles */

EXTERN line *lx_limit;

exbool tx_reserve(int want);
exbool lx_init(void);

/*
 * Named buffer routines.
 * These are implemented differently than the main buffer.
 * Each named buffer has a chain of blocks in the register file.
 * Each block contains roughly 508 chars of text,
 * and a previous and next block number.  We also have information
 * about which blocks came from deletes of multiple partial lines,
 * e.g. deleting a sentence or a LISP object.
 *
 * We maintain a free map for the temp file.  To free the blocks
 * in a register we must read the blocks to find how they are chained
 * together.
 *
 * The register file is a second heap block here, RNBLKS blocks of BUFSIZ, so
 * regio() points rbuf at one rather than seeking.
 */
#define RNBLKS 256

struct strreg {
    short rg_flags;
    short rg_nleft;
    short rg_first;
    short rg_last;
};

struct rbuf {
    short rb_prev;
    short rb_next;
    char rb_text[BUFSIZ - 2 * sizeof(short)];
};

EXTERN struct strreg strregs[('z' - 'a' + 1) + ('9' - '0' + 1)], *strp;
EXTERN struct rbuf *rbuf;
EXTERN short rused[RNBLKS / 16];
EXTERN short rnleft;
EXTERN short rblock;
EXTERN short rnext;
EXTERN char *rbufcp;

int partreg(int c);
