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
 * The two names, and the system-wide copy of each.  Upstream listed the
 * install directories its Makefile copied into; both files are compiled in
 * here (fileio.cpp serves them), so what is left is $HOME, /etc and the cwd.
 * /etc drops the leading dot: it is not a home directory.
 */
static char *pathname[] = { ".emacsrc", "emacs.hlp", "" };
static char *etcname[]  = { "/etc/emacs.rc", "/etc/emacs.hlp" };

#endif /* EPATH_H_ */
