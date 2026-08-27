/* Copyright (c) 1980 Regents of the University of California */
/* sccs id:	@(#)ex_tune.h	6.2 10/30/80  */
/*
 * Definitions of editor parameters and limits
 *
 * Upstream chose between a PDP-11 arm and a VMUNIX arm; wasm32 is the VMUNIX
 * one, so only those values are here. The pathnames are gone with the
 * preserve/recover helpers and the error-strings file.
 */

/*
 * If your system believes that tabs expand to a width other than
 * 8 then your makefile should cc with -DTABS=whatever, otherwise we use 8.
 */
#ifndef TABS
#define TABS 8
#endif

/*
 * Maximums
 *
 * The definition of LBSIZE should be the same as BUFSIZ (512 usually).
 * Most other definitions are quite generous.
 */
#define FNSIZE  128  /* File name size */
#define LBSIZE  1024 /* Line length */
#define ESIZE   512  /* Size of compiled re */
#define RHSSIZE 256  /* Size of rhs of substitute */
#define NBRA    9    /* Number of re \( \) pairs */
#define TAGSIZE 32   /* Tag length */
#define ONMSZ   64   /* Option name size */
#define GBSIZE  256  /* Buffer size */
#define UXBSIZE 128  /* Unix command buffer size */
#define VBSIZE  128  /* Partial line max size in visual */

/*
 * Arglist space. Upstream apologised for these being tiny off VMUNIX; they
 * are the generous arm.
 */
#define NCARGS 5120         /* Maximum arglist chars in "next" */
#define NARGS  (NCARGS / 6) /* Maximum number of names in "next" */

/*
 * The screen image. Upstream allocated TUBESIZE bytes on the stack in vop()
 * and never freed them, standing in for a non-portable alloca; here it is one
 * heap block (ex_vis.h), because vop() is a coroutine and a frame past 512
 * bytes costs a whole 64 KiB span. That makes it cheap to raise the limits to
 * what a maximised browser window wants: a grid may be SCREEN_MAX_COLS wide,
 * and the width here is that. The height is half SCREEN_MAX_ROWS, because
 * vlinfo's vliny is a char and a row number has to fit in one; a screen
 * taller than this is used down to line 128 and no further.
 */
#define TUBELINES 128   /* Number of screen lines for visual */
#define TUBECOLS  512   /* Number of screen columns for visual */
#define TUBESIZE  65536 /* Maximum screen size for visual */

/*
 * Output column (and line) are set to this value on cursor addressible
 * terminals when we lose track of the cursor to force cursor
 * addressing to occur.
 */
#define UKCOL -20 /* Prototype unknown column */

/*
 * Attention is the interrupt character (normally 0177 -- delete).
 * Quit is the quit signal (normally FS -- control-\) and quits open/visual.
 */
#define ATTN (-2)
#define QUIT ('\\' & 037)

/*
 * The cursor keys. A byte would not say which key was pressed and insert mode
 * has to know, so key_byte() answers one of these -- ATTN's trick of a value
 * no byte can hold -- and keycmd() turns it back into the command byte.
 * They must fit in a char: getkey() returns through one.
 */
#define KUP    (-3)
#define KDOWN  (-4)
#define KLEFT  (-5)
#define KRIGHT (-6)
#define KHOME  (-7)
#define KEND   (-8)
#define KPGUP  (-9)
#define KPGDN  (-10)
#define KDEL   (-11)

#define keynamed(c) ((c) <= KUP)
