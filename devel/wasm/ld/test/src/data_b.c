// Interior pointers into data_a.c, and a recursion that uses the stack.
#include "fixture.h"

extern const int data_squares[8];
extern int data_counts[4];
extern int data_zeros[16];
extern char data_big[65536];
extern char data_aligned[3];
extern const char data_text[];
extern int *data_b_mid;

int data_b_table[4] = {100, 200, 300, 400};
const int *data_sq5 = &data_squares[5];
int *data_cnt3 = &data_counts[3];
const char *data_tail = data_text + 7;

// A local array whose address escapes, so each frame is on the stack.
__attribute__((noinline)) static int sum_down(int n, int *prev)
{
    int here[4] = {n, n, n, prev ? *prev : 0};
    if (n == 0)
        return here[3];
    return n + sum_down(n - 1, here);
}

void fx_main(void)
{
    fx_puts("sq5 ");
    fx_putn(*data_sq5);
    fx_puts("\ncnt3 ");
    fx_putn(*data_cnt3);
    fx_puts("\nmid ");
    fx_putn(*data_b_mid);
    fx_puts("\ntail ");
    fx_puts(data_tail);
    fx_puts("\n");

    int zero = 1;
    for (int i = 0; i < 16; i++)
        zero &= data_zeros[i] == 0;
    for (int i = 0; i < 65536; i++)
        zero &= data_big[i] == 0;
    fx_puts(zero ? "bss zero\n" : "bss not zero\n");
    for (int i = 0; i < 65536; i++)
        data_big[i] = (char)(i * 7);
    int kept = 1;
    for (int i = 0; i < 65536; i++)
        kept &= data_big[i] == (char)(i * 7);
    fx_puts(kept ? "bss writable\n" : "bss not writable\n");

    fx_puts((unsigned long)data_aligned % 64 == 0 ? "aligned " : "misaligned ");
    fx_putn(data_aligned[0] + data_aligned[1] + data_aligned[2]);
    fx_puts("\nstack ");
    fx_putn(sum_down(20, 0));
    fx_puts("\n");
}
