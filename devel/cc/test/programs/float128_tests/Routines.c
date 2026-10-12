long double third(void) { return 1; }
int main(void)
{
    long double tiny = 1;
    for (int i = 0; i < 100; i++)
        tiny /= 2;
    // 113 bits of precision: 1 + 2^-100 differs from 1.
    if (ld_sub(1 + tiny, 1) != tiny) return 1;
    if (ld_sub(2.5L, 0.25L) != 2.25L) return 2;
    if (ld_split(3, 0, 0, 0, 0, 0, 4, 1.5L) != 9) return 3;
    if (ld_stack(0, 0, 0, 0, 0, 0, 0, 0, 4, 10) != 2.5L) return 4;
    if (ld_vsum(3, 1.5L, tiny, 2.0L) != 3.5L + tiny) return 5;
    if (ld_vmix(1, 2, 3, 0.5L, 7, 9.0L) != 5715) return 6;
    struct LD s = { 1.25L };
    s = ld_struct(s, 4);
    if (s.x != 5) return 7;
    struct A16 t = { 3, 4 };
    if (a16_stack(0, 0, 0, 0, 0, 0, 0, 0, 5, t) != 534) return 8;
    if (a16_va(2, t) != 234) return 9;
    if (ld_conv(0.5, 0.25f, -3, 1ULL << 63) != (long double)(1ULL << 63) - 2.25L) return 10;
    if (ld_back(-7.75L, 0) != -7) return 11;
    if (ld_back(1e18L, 1) != 1000000000000000000) return 12;
    if (ld_back(2.5L, 2) != 10) return 13;
    long double x = third() / 3;
    if (x * 3 != 1 || -x >= 0 || !(x > 0.333L) || x == (double)x) return 14;
    return 0;
}

