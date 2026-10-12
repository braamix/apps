//
// Memory allocation.
//
// Copyright (c) 2025 Serge Vakulenko
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
#include "xalloc.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "srcloc.h"

int xalloc_debug; // Enable manually for debug

//
// Structure for the header of each allocated block
//
typedef struct BlockHeader {
    struct BlockHeader *next;
    struct BlockHeader *prev;
    size_t requested_size;
    const char *funcname;
    const char *filename;
    unsigned lineno;
} BlockHeader;

//
// Global pointer to the head of the doubly linked list
//
static BlockHeader *head = NULL;

//
// Allocate size bytes and return a pointer to the memory.
// Exits the program with an error message if allocation fails.
// Every allocation is tracked in an internal list so leaks can be reported.
// Arguments funcname, filename, and lineno identify the call site
// (caller's __func__, __FILE__, __LINE__); they are stored in the block header
// and printed by xreport_lost_memory() so you can pinpoint which allocation
// was never freed.
//
void *xalloc(size_t size, const char *funcname, const char *filename, unsigned lineno)
{
    /* Calculate total size: header + requested size */
    size_t total_size = sizeof(BlockHeader) + size;

    /* Allocate memory using malloc */
    void *ptr = calloc(1, total_size);
    if (ptr == NULL) {
        diag_error(diag_loc, "out of memory: %zu bytes for %s() at %s:%u", size, funcname, filename,
                   lineno);
        exit(1);
    }

    /* Set up the header */
    BlockHeader *h    = (BlockHeader *)ptr;
    h->requested_size = size;
    h->funcname       = funcname;
    h->filename       = filename;
    h->lineno         = lineno;

    /* Insert into the doubly linked list */
    if (head == NULL) {
        /* First allocation */
        head = h;
    } else {
        /* Insert at the head of the list */
        h->next    = head;
        head->prev = h;
        head       = h;
    }

    /* Return pointer to user data (after header) */
    ptr = (void *)((char *)ptr + sizeof(BlockHeader));
    if (xalloc_debug) {
        printf("--- %s %zu bytes, %p\n", __func__, size, ptr);
    }
    return ptr;
}

//
// Free memory previously returned by xalloc() or xstrdup().
// Safe to call with NULL. Removes the block from the tracking list.
//
void xfree(void *ptr)
{
    if (ptr == NULL) {
        return; /* Nothing to free */
    }

    /* Get the header (before the user pointer) */
    BlockHeader *h = (BlockHeader *)((char *)ptr - sizeof(BlockHeader));
    if (xalloc_debug) {
        printf("--- %s %zu bytes, %p\n", __func__, h->requested_size, ptr);
    }

    /* Remove from the doubly linked list */
    if (h->prev != NULL) {
        if (h->prev->next != h) {
            internal_error("xfree: the list of blocks is damaged before %p", ptr);
        }
        h->prev->next = h->next;
    } else {
        /* This is the head */
        if (head != h) {
            internal_error("xfree: the head of the list of blocks is damaged at %p", ptr);
        }
        head = h->next;
    }

    if (h->next != NULL) {
        if (h->next->prev != h) {
            internal_error("xfree: the list of blocks is damaged after %p", ptr);
        }
        h->next->prev = h->prev;
    }
    // Just in case.
    h->next = NULL;
    h->prev = NULL;

    /* Free the entire block (header + user data) */
    free(h);
}

//
// Print a list of all memory that was allocated but never freed.
// Call this at the end of the program to check for leaks.
//
void xreport_lost_memory()
{
    if (head) {
        printf("Lost memory:\n");
    }
    for (const BlockHeader *h = head; h; h = h->next) {
        const char *filename = strrchr(h->filename, '/');
        if (!filename)
            filename = h->filename;
        else
            filename++;
        printf("%zu bytes allocated by %s() at line %u of file %s\n", h->requested_size,
               h->funcname, h->lineno, filename);
    }
}

//
// Return the total number of bytes currently allocated (not yet freed).
// Useful in tests to verify that all memory was released.
//
size_t xtotal_allocated_size()
{
    size_t total = 0;
    for (const BlockHeader *h = head; h; h = h->next) {
        total += h->requested_size;
    }
    return total;
}

//
// Free every allocation at once, without requiring individual xfree() calls.
// Use at program exit when releasing each block individually is not practical.
//
void xfree_all()
{
    if (xalloc_debug) {
        printf("--- %s\n", __func__);
    }
    while (head) {
        BlockHeader *next = head->next;
        free(head);
        head = next;
    }
}

//
// Return a freshly allocated copy of str, or NULL if str is NULL.
// The copy must be released with xfree() when no longer needed.
//
char *xstrdup(const char *str)
{
    if (!str)
        return NULL;
    char *new_str = xalloc(strlen(str) + 1, __func__, __FILE__, __LINE__);
    strcpy(new_str, str);
    return new_str;
}

//
// Return a freshly allocated copy of the first len bytes of data, or NULL if data is NULL.
// The length-carrying counterpart of xstrdup(): the copy may hold embedded NUL bytes, and
// one extra NUL is appended so it is still safe to print when it happens to be text.
// The copy must be released with xfree() when no longer needed.
//
char *xmemdup(const void *data, size_t len)
{
    if (!data)
        return NULL;
    char *copy = xalloc(len + 1, __func__, __FILE__, __LINE__);
    memcpy(copy, data, len);
    copy[len] = '\0';
    return copy;
}

//
// Build a unique name by combining prefix with a counter, then increment the counter.
// For example, xstruniq("%", &n) returns "%0", "%1", "%2", … on successive calls.
// The caller owns the counter: declare it where the name series should start and reset,
// initialize it to zero, and reset it to zero whenever a fresh series is needed.
// The returned string is heap-allocated and must be released with xfree().
//
char *xstruniq(const char *prefix, int *counter)
{
    char name[64];
    snprintf(name, sizeof name, "%s%d", prefix, (*counter)++);
    return xstrdup(name);
}
