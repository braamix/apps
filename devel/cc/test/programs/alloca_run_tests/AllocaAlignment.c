#include <alloca.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

int main(void)
{
    static const int sizes[] = { 1, 3, 0, 5, 7, 2 };
    char *p[6];
    int aligned = 1, apart = 1;
    for (int i = 0; i < 6; i++) {
        p[i] = alloca(sizes[i]);
        if ((uintptr_t)p[i] % _Alignof(max_align_t) != 0)
            aligned = 0;
        for (int j = 0; j < sizes[i]; j++)
            p[i][j] = (char)i;
    }
    for (int i = 0; i < 6; i++)
        for (int j = 0; j < sizes[i]; j++)
            if (p[i][j] != i)
                apart = 0;
    printf("aligned %d apart %d\n", aligned, apart);
    return 0;
}

