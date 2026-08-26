/* citrus_bcs.h has its own ASCII-only versions and uses those; these are for
 * the two modules that reach for the C names. */
#ifndef _CTYPE_H_
#define _CTYPE_H_

extern "C" {
int isspace(int c);
int isdigit(int c);
int isalpha(int c);
int isalnum(int c);
int isupper(int c);
int islower(int c);
int toupper(int c);
int tolower(int c);
}

#endif
