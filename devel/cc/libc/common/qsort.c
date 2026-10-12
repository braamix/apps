// qsort — heapsort (C11 §7.22.5.2): no recursion and no extra memory, and, as
// C allows, not stable.
#include <stdlib.h>

static void swap(char *a, char *b, size_t size)
{
    while (size--) {
        char t = *a;
        *a++   = *b;
        *b++   = t;
    }
}

// Moves base[root] down the heap of n elements until both children are smaller.
static void sift(char *base, size_t root, size_t n, size_t size,
                 int (*compar)(const void *, const void *))
{
    for (;;) {
        size_t child = 2 * root + 1;
        if (child >= n)
            return;
        if (child + 1 < n && compar(base + child * size, base + (child + 1) * size) < 0)
            child++;
        if (compar(base + root * size, base + child * size) >= 0)
            return;
        swap(base + root * size, base + child * size, size);
        root = child;
    }
}

void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *))
{
    char *b = base;
    if (nmemb < 2 || size == 0)
        return;
    for (size_t k = nmemb / 2; k-- > 0;)
        sift(b, k, nmemb, size, compar);
    for (size_t n = nmemb - 1; n > 0; n--) {
        swap(b, b + n * size, size);
        sift(b, 0, n, size, compar);
    }
}
