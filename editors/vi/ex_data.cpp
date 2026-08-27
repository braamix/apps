/* Copyright (c) 1980 Regents of the University of California */
/* @(#)ex_data.c	6.1 10/18/80 */

/*
 * Initialization of option values, and the one definition of every global.
 *
 * Upstream declared its globals in the headers without extern, relying on C's
 * common symbols; C++ has none, so the headers declare and this file defines.
 * EX_DEFINE is what empties the EXTERN macro.
 *
 * The option #defines in ex_vars.h are made from this file by the script
 * makeoptions, which is not in the distribution.
 *
 * These initializations are done char by char instead of as strings
 * to confuse xstr so it will leave them alone.
 */
#define EX_DEFINE

#include "ex.h"
#include "ex_argv.h"
#include "ex_buf.h"
#include "ex_re.h"
#include "ex_screen.h"
#include "ex_vis.h"

char direct[ONMSZ]            = { '/', 't', 'm', 'p' };
static char paragraphs[ONMSZ] = {
    'I', 'P', 'L', 'P', 'P', 'P', 'Q', 'P', /* -ms macros */
    'P', ' ', 'L', 'I',                     /* -mm macros */
    'b', 'p'                                /* bare nroff */
};
static char sections[ONMSZ] = {
    'N', 'H', 'S', 'H', /* -ms macros */
    'H', ' ', 'H', 'U'  /* -mm macros */
};
char shell[ONMSZ]       = { '/', 'b', 'i', 'n', '/', 's', 'h' };
static char tags[ONMSZ] = { 't', 'a', 'g', 's' };
/*
 * There is no terminal type: the screen is a grid of cells, so a name would
 * name nothing. "braam" is what :set term prints, and setting it is an error.
 */
char ttytype[ONMSZ] = { 'b', 'r', 'a', 'a', 'm' };

/*
 * The capability objects. Only their addresses matter: a non-null one says the
 * screen can do this, and putpad() acts on SO and SE alone.
 */
char CAP_YES[] = "y";
char CAP_SO[]  = "y";
char CAP_SE[]  = "y";

short COLUMNS = 80;
short LINES   = 24;

struct option options[NOPTS + 1] = {
    { "autoindent", "ai", ONOFF, 0, 0, 0 },
    { "autoprint", "ap", ONOFF, 1, 1, 0 },
    { "autowrite", "aw", ONOFF, 0, 0, 0 },
    { "beautify", "bf", ONOFF, 0, 0, 0 },
    { "directory", "dir", STRING, 0, 0, direct },
    { "edcompatible", "ed", ONOFF, 0, 0, 0 },
    { "errorbells", "eb", ONOFF, 0, 0, 0 },
    { "hardtabs", "ht", NUMERIC, 8, 8, 0 },
    { "ignorecase", "ic", ONOFF, 0, 0, 0 },
    { "lisp", 0, ONOFF, 0, 0, 0 },
    { "list", 0, ONOFF, 0, 0, 0 },
    { "magic", 0, ONOFF, 1, 1, 0 },
    { "mesg", 0, ONOFF, 1, 1, 0 },
    { "number", "nu", ONOFF, 0, 0, 0 },
    { "open", 0, ONOFF, 1, 1, 0 },
    { "optimize", "opt", ONOFF, 0, 0, 0 },
    { "paragraphs", "para", STRING, 0, 0, paragraphs },
    { "prompt", 0, ONOFF, 1, 1, 0 },
    { "readonly", "ro", ONOFF, 0, 0, 0 },
    { "redraw", 0, ONOFF, 0, 0, 0 },
    { "remap", 0, ONOFF, 1, 1, 0 },
    { "report", 0, NUMERIC, 5, 5, 0 },
    { "scroll", "scr", NUMERIC, 12, 12, 0 },
    { "sections", "sect", STRING, 0, 0, sections },
    { "shell", "sh", STRING, 0, 0, shell },
    { "shiftwidth", "sw", NUMERIC, TABS, TABS, 0 },
    { "showmatch", "sm", ONOFF, 0, 0, 0 },
    { "slowopen", "slow", ONOFF, 0, 0, 0 },
    { "tabstop", "ts", NUMERIC, TABS, TABS, 0 },
    { "taglength", "tl", NUMERIC, 0, 0, 0 },
    { "tags", "tag", STRING, 0, 0, tags },
    { "term", 0, OTERM, 0, 0, ttytype },
    { "terse", 0, ONOFF, 0, 0, 0 },
    { "timeout", "to", ONOFF, 1, 1, 0 },
    { "ttytype", "tty", OTERM, 0, 0, ttytype },
    { "warn", 0, ONOFF, 1, 1, 0 },
    { "window", "wi", NUMERIC, 23, 23, 0 },
    { "wrapscan", "ws", ONOFF, 1, 1, 0 },
    { "wrapmargin", "wm", NUMERIC, 0, 0, 0 },
    { "writeany", "wa", ONOFF, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0 },
};
