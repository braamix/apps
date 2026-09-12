// The POSIX ERE surface eh uses. No regerror(): eh only tests regcomp != 0.
#pragma once

#include <stddef.h>

enum {
    REG_EXTENDED = 1,
    REG_ICASE    = 2,
    REG_NEWLINE  = 4,
    REG_NOSUB    = 8,
};

enum {
    REG_NOTBOL = 1,
    REG_NOTEOL = 2,
};

enum {
    REG_NOMATCH = 1,
    REG_BADPAT  = 2,
    REG_ESPACE  = 3,
};

typedef ptrdiff_t regoff_t;

typedef struct {
    regoff_t rm_so;
    regoff_t rm_eo;
} regmatch_t;

// POD: eh keeps one at namespace scope, where a destructor cannot run.
typedef struct {
    size_t re_nsub;

    void *prog;   // node arena
    void *sets;   // bracket arena
    void *ranges; // non-ASCII ranges the brackets name
    int nnodes, nsets, nranges;
    int start; // first node, -1 when the pattern is empty
    int cflags;

    // What regexec skips start positions with.
    unsigned char first[32]; // bytes a match can begin with
    int have_first;
    int anchored; // ^ leads: only a line start can match
} regex_t;

int regcomp(regex_t *preg, const char *pattern, int cflags);
int regexec(const regex_t *preg, const char *string, size_t nmatch, regmatch_t pmatch[],
            int eflags);
void regfree(regex_t *preg);
