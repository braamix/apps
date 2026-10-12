#include <alloca.h>
#include <setjmp.h>
#include <stdio.h>

static jmp_buf env;

static void deep(int n)
{
    char *p = alloca(200);
    p[0]   = 1;
    p[199] = (char)n;
    if (p[0] + p[199] == 1 + (char)n)
        longjmp(env, 1);
}

coro(braam_call *) int main(void)
{
    int count = 0;
    for (int i = 0; i < 20000; i++) {
        volatile int jumped = 0;
        if (setjmp(env) == 0)
            deep(i);
        else
            jumped = 1;
        count += jumped;
    }
    await printf("%d\n", count);
    return 0;
}

