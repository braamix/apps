/*
 * A file in memory, and the sequential reads the a.out loader makes of it.
 *
 * Copyright (c) 2026, Serge Vakulenko
 */
#include "besm6_defs.h"

int blob_getc(Blob *b)
{
    return b->pos < b->len ? b->base[b->pos++] : -1;
}

size_t blob_read(Blob *b, void *p, size_t n)
{
    size_t left = b->len - b->pos;

    if (n > left)
        n = left;
    memcpy(p, b->base + b->pos, n);
    b->pos += n;
    return n;
}

char *blob_gets(Blob *b, char *s, int n)
{
    int i = 0;

    if (b->pos >= b->len)
        return NULL;
    while (i < n - 1 && b->pos < b->len) {
        int c  = b->base[b->pos++];
        s[i++] = (char)c;
        if (c == '\n')
            break;
    }
    s[i] = 0;
    return s;
}

void blob_rewind(Blob *b)
{
    b->pos = 0;
}
