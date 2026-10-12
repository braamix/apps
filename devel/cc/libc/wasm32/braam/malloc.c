/*
 * The allocator of a Braam process (docs/Braam.md §7.2): first fit over a free
 * list kept in address order, neighbours merged when a block is freed, the linear memory
 * grown when nothing fits.  Braam frees every reply block, so the bump allocator of
 * plain wasm32 would grow without end.
 *
 * A block is a 16-byte header, its size (the header included, a multiple of 16) and
 * whether it is in use, then the bytes, 16-aligned.  A free block holds the next free
 * one in its first word.
 */
#include <stddef.h>
#include <string.h>

#define PAGE  65536UL
#define ALIGN 16

extern char __heap_base[];
unsigned long __vcc_memory_size(void);
long __vcc_memory_grow(unsigned long pages);

typedef struct block {
    size_t size;
    size_t used;
    size_t pad[2];
} Block;

static char *brk;        /* the end of the heap so far */
static Block *free_list; /* in address order */

static Block **next_of(Block *b)
{
    return (Block **)(b + 1);
}

/* A new block of `size` bytes at the end of the heap, or NULL. */
static Block *extend(size_t size)
{
    if (!brk)
        brk = (char *)(((size_t)__heap_base + ALIGN - 1) & ~(size_t)(ALIGN - 1));
    size_t end  = (size_t)brk + size;
    size_t have = __vcc_memory_size() * PAGE;
    if (end < (size_t)brk)
        return NULL;
    if (end > have && __vcc_memory_grow((end - have + PAGE - 1) / PAGE) < 0)
        return NULL;
    Block *b = (Block *)brk;
    b->size  = size;
    brk      = (char *)end;
    return b;
}

void *malloc(size_t n)
{
    if (n > (size_t)-1 - 2 * ALIGN)
        return NULL;
    size_t size  = (n + sizeof(Block) + ALIGN - 1) & ~(size_t)(ALIGN - 1);
    Block **link = &free_list;
    Block *b;
    for (b = free_list; b; link = next_of(b), b = *link)
        if (b->size >= size)
            break;
    if (b) {
        *link = *next_of(b);
        if (b->size >= size + 2 * sizeof(Block)) {
            /* Split: the rest stays free, where b was in the list. */
            Block *rest    = (Block *)((char *)b + size);
            rest->size     = b->size - size;
            rest->used     = 0;
            *next_of(rest) = *link;
            *link          = rest;
            b->size        = size;
        }
    } else if (!(b = extend(size))) {
        return NULL;
    }
    b->used = 1;
    return b + 1;
}

void free(void *p)
{
    if (!p)
        return;
    Block *b    = (Block *)p - 1;
    b->used     = 0;
    Block *prev = NULL, *next = free_list;
    while (next && next < b) {
        prev = next;
        next = *next_of(next);
    }
    /* Merge with the next free block, then the previous one. */
    if (next && (char *)b + b->size == (char *)next) {
        b->size += next->size;
        next = *next_of(next);
    }
    *next_of(b) = next;
    if (prev && (char *)prev + prev->size == (char *)b) {
        prev->size += b->size;
        *next_of(prev) = next;
    } else if (prev) {
        *next_of(prev) = b;
    } else {
        free_list = b;
    }
}

void *calloc(size_t n, size_t size)
{
    if (size != 0 && n > (size_t)-1 / size)
        return NULL;
    void *p = malloc(n * size);
    if (p)
        memset(p, 0, n * size);
    return p;
}

void *realloc(void *p, size_t n)
{
    if (!p)
        return malloc(n);
    Block *b    = (Block *)p - 1;
    size_t have = b->size - sizeof(Block);
    if (n <= have)
        return p;
    void *q = malloc(n);
    if (q) {
        memcpy(q, p, have);
        free(p);
    }
    return q;
}
