#ifndef _STRINGS_H_
#define _STRINGS_H_

#include <sys/types.h>

extern "C" {
void bzero(void *d, size_t n);
int strcasecmp(const char *a, const char *b);
int strncasecmp(const char *a, const char *b, size_t n);
char *strcasestr(const char *h, const char *n);
}

#endif
