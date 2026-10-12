#include <coro.h>
#include <stdio.h>
co_frame(int, int) *self;
coro(int) int g(int n) { if (n == 9) co_resume(self); yield n; return n; }
_Alignas(16) char buf[128];
int main(void)
{
    co_frame(int, int) *f;
    TRAP
    puts("not reached");
    return 0;
}

