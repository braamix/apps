#include <coro.h>
#include <stdio.h>

static void f(int x)
{
    int n = 3;
    defer {
        defer puts("in");
        for (int i = 0; i < 10; i++) {
            if (i == n)
                break;
            printf("%d ", i);
        }
        switch (n) {
        case 2:
        case 3:
            printf("two ");
            break;
        default:
            printf("other ");
        }
        goto done;
    done:
        printf("| ");
    }
    if (x == 1)
        return;
    if (x == 2)
        n = 2;
}

int main(void)
{
    f(0);
    f(1);
    f(2);
    return 0;
}

