struct s {
    int i : 6;
    unsigned u : 3;
    long l : 20;
    unsigned w : 7;
};

int main(void) {
    struct s x = { 0, 0, 0, 0 };
    struct s *p = &x;
    x.i = 10;
    x.i *= -3;              // -30
    if (x.i != -30)
        return 1;
    x.i -= 5;               // -35 wraps to 29
    if (x.i != 29)
        return 2;
    p->u = 6;
    p->u /= -2;             // int division: -3, stored as 5
    if (p->u != 5)
        return 3;
    if (x.u - 7 >= 0)       // promoted to int: -2
        return 4;
    p->l = 1000;
    p->l <<= 10;            // 1024000 wraps to -24576 in 20 bits
    if (p->l != -24576l)
        return 5;
    p->l >>= 4;
    if (p->l != -1536)
        return 6;
    x.w = 126;
    if (x.w++ != 126 || x.w != 127)
        return 7;
    if (++x.w != 0 || x.w != 0)   // wraps
        return 8;
    if (x.w-- != 0 || x.w != 127)
        return 9;
    p->i = 31;
    if (++p->i != -32)
        return 10;
    if (p->i-- != -32 || p->i != 31)
        return 11;
    // The value of an assignment is the value the field holds afterwards.
    if ((x.u = 9) != 1)
        return 12;
    if ((p->i = 40) != -24)
        return 13;
    if ((x.w |= 0x80) != 127)
        return 14;
    int sum = x.i + x.u + x.w;   // -24 + 1 + 127
    if (sum != 104)
        return 15;
    return 0;
}
