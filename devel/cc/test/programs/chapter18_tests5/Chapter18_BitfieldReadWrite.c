struct s {
    unsigned a : 3;
    int b : 5;
    char c;
    int d : 12;
    unsigned long e : 20;
    long f : 31;
    _Bool g : 1;
    unsigned h : 1;
};

int check(struct s *p, unsigned a, int b, char c, int d, unsigned long e, long f, int g, unsigned h)
{
    return p->a == a && p->b == b && p->c == c && p->d == d && p->e == e && p->f == f &&
           p->g == g && p->h == h;
}

int main(void) {
    struct s x;
    struct s *p = &x;
    x.a = 5;
    x.b = -7;
    x.c = 'q';
    x.d = -1000;
    x.e = 0xabcdeul;
    x.f = -123456789l;
    x.g = 1;
    x.h = 1;
    if (!check(&x, 5, -7, 'q', -1000, 0xabcdeul, -123456789l, 1, 1))
        return 1;
    // Change each field in turn; the others must not move.
    p->a = 2;
    if (!check(&x, 2, -7, 'q', -1000, 0xabcdeul, -123456789l, 1, 1))
        return 2;
    p->b = 15;
    if (!check(&x, 2, 15, 'q', -1000, 0xabcdeul, -123456789l, 1, 1))
        return 3;
    p->d = 2047;
    if (!check(&x, 2, 15, 'q', 2047, 0xabcdeul, -123456789l, 1, 1))
        return 4;
    p->e = 0;
    if (!check(&x, 2, 15, 'q', 2047, 0, -123456789l, 1, 1))
        return 5;
    p->f = 1073741823l;
    if (!check(&x, 2, 15, 'q', 2047, 0, 1073741823l, 1, 1))
        return 6;
    x.g = 0;
    x.h = 0;
    if (!check(&x, 2, 15, 'q', 2047, 0, 1073741823l, 0, 0))
        return 7;
    // Truncation to the field.
    x.a = 13;      // 0b1101
    x.b = 33;      // 0b100001
    x.d = 4096 + 5;
    x.e = 0x123456ul;
    if (x.a != 5 || x.b != 1 || x.d != 5 || x.e != 0x23456ul)
        return 8;
    x.b = 16;      // the sign bit alone
    x.d = 2048;
    if (x.b != -16 || x.d != -2048)
        return 9;
    // A conversion to _Bool is a zero test, not a truncation.
    x.g = 2;
    if (x.g != 1)
        return 10;
    if (x.c != 'q')
        return 11;
    return 0;
}
