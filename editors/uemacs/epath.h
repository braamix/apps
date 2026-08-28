/*	EPATH.H
 *
 *	This file contains certain info needed to locate the
 *	initialization (etc) files on a system dependent basis
 *
 *	modified by Petri Kutvonen
 */
#ifndef EPATH_H_
#define EPATH_H_

/*
 * The two names, the /etc copy of each, and the leaf each has in the package.
 * Upstream's install directories became one found at startup; both spellings
 * drop the dot, as neither place is a home directory.
 */
static char *pathname[] = { ".emacsrc", "emacs.hlp", "" };
static char *etcname[]  = { "/etc/emacs.rc", "/etc/emacs.hlp" };
static char *sysname[]  = { "emacs.rc", "emacs.hlp" }; /* under datadir */

extern char datadir[]; /* the package's share, empty for none (epath.cpp) */

#endif /* EPATH_H_ */
