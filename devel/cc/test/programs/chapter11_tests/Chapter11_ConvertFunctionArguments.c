int foo(long a, int b, int c, int d, long e, int f, long g, int h) {
    if (a != -1l)
        return 1;
    if (b != 2)
        return 2;
    if (c != 0)
        return 3;
    if (d != -5)
        return 4;
    if (e != -101l)
        return 5;
    if (f != -123)
        return 6;
    if (g != -10l)
        return 7;
    if (h != 1234)
        return 8;
    return 0;
}

int main(void) {
    int a = -1;
    long int b = 4294967298;
    long c = -4294967296;
    long d = 21474836475;
    int e = -101;
    long f = -123;
    int g = -10;
    long h = -9223372036854774574;
    return foo(a, b, c, d, e, f, g, h);
}
