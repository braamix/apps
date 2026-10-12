#include <coro.h>

struct S { int a, b, c, d; };

static int scalar(void) { int r = 1; defer r = 2; return r; }
static struct S aggregate(void) { struct S s = { 1, 2, 3, 4 }; defer s.a = 9; return s; }
static int through(int *p) { defer *p = 7; return *p; }

int main(void)
{
    int v = 3;
    struct S s = aggregate();
    if (scalar() != 1) return 1;
    if (s.a != 1 || s.d != 4) return 2;
    if (through(&v) != 3 || v != 7) return 3;
    return 0;
}

