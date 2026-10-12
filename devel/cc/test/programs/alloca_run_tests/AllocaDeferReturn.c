#include <alloca.h>
#include <coro.h>
#include <stdio.h>

static int f(int early)
{
    int *v = alloca(3 * sizeof(int));
    v[0] = 5;
    v[1] = 7;
    v[2] = 18;
    defer printf("defer %d\n", v[1]);
    if (early) {
        for (int i = 0; i < 3; i++)
            if (v[i] == 7)
                return v[0] + v[i];
    }
    return v[1] + v[2] + v[0];
}

int main(void)
{
    int a = f(1);
    int b = f(0);
    printf("first %d then %d\n", a, b);
    return 0;
}

