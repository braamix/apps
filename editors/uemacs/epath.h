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
 * The two names, and where else to look for them.  Upstream listed the Unix
 * install directories the Makefile copied the files into; there is no install
 * step here and both files are compiled into the binary (fileio.cpp serves
 * them when nothing on disk answers), so what is left is the cwd and $HOME,
 * which is where a user's own copy would be.
 */
static char *pathname[] = { ".emacsrc", "emacs.hlp", "" };

#endif /* EPATH_H_ */
