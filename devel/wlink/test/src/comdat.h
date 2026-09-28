// Emitted into every object that uses it: one copy must survive the link.
#pragma once

__attribute__((noinline)) inline int *comdat_counter()
{
    static int count;
    return &count;
}

template <int N> __attribute__((noinline)) int comdat_scaled(int x)
{
    return x * N + *comdat_counter();
}

int comdat_b_bump();
int *comdat_b_counter();
