// .rodata, .data and .bss, an aligned variable and a large zeroed array.
#include "fixture.h"

const int data_squares[8] = {0, 1, 4, 9, 16, 25, 36, 49};
int data_counts[4] = {10, 20, 30, 40};
int data_zeros[16];
char data_big[65536];
__attribute__((aligned(64))) char data_aligned[3] = {1, 2, 3};
const char data_text[] = "0123456789";

// Pointers with addends, into data_b.c's objects from here.
extern int data_b_table[];
int *data_b_mid = &data_b_table[2];
