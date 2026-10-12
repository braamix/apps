#include <coro.h>
#include <stdio.h>

static int twice(int *p) { return *p * 2; }

static coro(double) long double mix(long double base, int n)
{
    double acc = 0.5;
    int arr[5] = { 1, 2, 3, 4, 5 };
    int *q     = arr;
    long double total = base;
    for (int i = 0; i < n; i++) {
        switch (i % 3) {
        case 0:
            acc = acc * 2 + (yield acc) + twice(q);
            break;
        case 1:
            for (int j = 0; j < 2; j++) {
                if (j == 1)
                    goto skip;
                yield -acc;
            }
        skip:
            q++;
            break;
        default:
            total += *q + (yield acc + 0.25);
        }
    }
    return total;
}

int main(void)
{
    co_frame(double, long double) *m = co_alloca(mix, 0, 1.5L, 7);
    int n = 0;
    while (co_resume(m) == CO_SUSPENDED)
        printf("%g ", co_value(m)), n++;
    printf("| %d %g\n", n, (double)co_result(m));
    return 0;
}

