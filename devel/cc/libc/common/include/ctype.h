/*
 * <ctype.h> — character handling (C11 §7.4).
 *
 * All functions are declared for future implementation (TODO).  On BESM-6 the
 * console uses KOI7; an implementation there should classify against KOI7.
 * Arguments and results follow ISO C: the argument is an int representable as
 * unsigned char or EOF.
 */
#ifndef _CTYPE_H
#define _CTYPE_H

int isalnum(int c);
int isalpha(int c);
int isblank(int c);
int iscntrl(int c);
int isdigit(int c);
int isgraph(int c);
int islower(int c);
int isprint(int c);
int ispunct(int c);
int isspace(int c);
int isupper(int c);
int isxdigit(int c);

int tolower(int c);
int toupper(int c);

#endif /* _CTYPE_H */
