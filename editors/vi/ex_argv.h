/* Copyright (c) 1980 Regents of the University of California */
/* sccs id:	@(#)ex_argv.h	6.1 10/18/80  */
/*
 * The current implementation of the argument list is poor,
 * using an argv even for internally done "next" commands.
 * It is not hard to see that this is restrictive and a waste of
 * space.  The statically allocated glob structure could be replaced
 * by a dynamically allocated argument area space.
 */
#pragma once

EXTERN char **argv;
EXTERN char **argv0;
EXTERN char *args;
EXTERN char *args0;
EXTERN short argc;
EXTERN short argc0;
EXTERN short morargc; /* Used with "More files to edit..." */

EXTERN int firstln;    /* From +lineno */
EXTERN char *firstpat; /* From +/pat	*/

/* Yech... */
struct glob {
    short argc;            /* Index of current file in argv */
    short argc0;           /* Number of arguments in argv */
    char *argv[NARGS + 1]; /* WHAT A WASTE! */
    char argspac[NCARGS + sizeof(int)];
};

EXTERN struct glob frob;
