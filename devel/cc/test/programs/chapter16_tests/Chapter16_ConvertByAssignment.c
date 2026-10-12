/* Test implicit conversions to and from character types as if by assignment. */

int check_int(int converted, int expected) { return (converted == expected); }
int check_uint(unsigned int converted, unsigned int expected) { return (converted == expected); }
int check_long(long converted, long expected) { return (converted == expected); }
int check_ulong(unsigned long converted, unsigned long expected) { return (converted == expected); }
int check_double(double converted, double expected) { return (converted == expected); }
int check_char(char converted, char expected) { return (converted == expected); }
int check_uchar(unsigned char converted, unsigned char expected) { return (converted == expected); }
int check_stk(signed char expected, int dummy1, int dummy2, int dummy3,
                        int dummy4, int dummy5, int dummy6, signed char converted) {
    return converted == expected;
}

int rxt_uc(unsigned char c) { return c; }
unsigned long rxt_sc(signed char sc) { return sc; }
unsigned char rtrunc(long l) { return l; }

int main(void) {
    signed char sc = -10;
    if (!check_long(sc, -10l)) return 1;
    if (!check_uint(sc, 4294967286u)) return 2;
    if (!check_double(sc, -10.0)) return 3;

    unsigned char uc = 246;
    if (!check_uchar(sc, uc)) return 4;

    char c = -10;
    if (!check_char(-10, c)) return 5;
    if (!check_char(4294967286u, c)) return 6;
    // if (!check_char(-10.0, c)) return 7; -- undefined behavior on ARM
    if (!check_stk(c, 0, 0, 0, 0, 0, 0, -10.0)) return 8;

    if (!check_int(uc, 246)) return 9;
    if (!check_ulong(uc, 246ul)) return 10;
    char expected_char = -10;
    if (!check_char(uc, expected_char)) return 11;

    if (!check_uchar(18446744073709551606ul, uc)) return 12;

    if (rxt_uc(uc) != 246) return 13;
    if (rxt_sc(sc) != 18446744073709551606ul) return 14;
    if (rtrunc(5369233654l) != uc) return 15;

    signed char array[3] = {0, 0, 0};
    array[1] = 128;
    if (array[0] || array[2] || array[1] != -128) return 16;
    array[1] = 9224497936761618562ul;
    if (array[0] || array[2] || array[1] != -126) return 17;
    array[1] = -2.6;
    if (array[0] || array[2] || array[1] != -2) return 18;

    unsigned char uchar_array[3] = {0, 0, 0};
    uchar_array[1] = 17592186044416l;
    if (uchar_array[0] || uchar_array[2] || uchar_array[1] != 0) return 19;
    uchar_array[1] = 2147483898u;
    if (uchar_array[0] || uchar_array[2] || uchar_array[1] != 250) return 20;

    unsigned int ui = 4294967295U;
    static unsigned char uc_static;
    ui = uc_static;
    if (ui) return 21;

    signed long l = -1;
    static signed s_static = 0;
    l = s_static;
    if (l) return 22;

    return 0;
}
