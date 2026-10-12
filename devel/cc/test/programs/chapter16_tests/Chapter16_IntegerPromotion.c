/* Test that we promote character types to integers when we're required to */

int add_chars(char c1, char c2, char c3) {
    return c1 + c2 + c3;
}

int negate(unsigned char uc) {
    return -uc;
}

int complement(unsigned char uc) {
    return ~uc;
}

int add_then_div(signed char a, signed char b, signed char c) {
    return (a + b) / c;
}

int mixed_multiply(signed char s, unsigned char u) {
    return s * u;
}

signed char decrement(signed char s) {
    s = s - 1;
    return s;
}

int main(void) {
    char a = 100;
    char b = 109;
    if (add_chars(a, a, b) != 309) {
        return 1;
    }

    unsigned char one = 1;
    if (negate(one) != -1) {
        return 2;
    }

    if (complement(one) != -2) {
        return 3;
    }

    signed char w = 127;
    signed char x = 3;
    signed char y = 2;
    if (add_then_div(w, x, y) != 65)
        return 4;

    signed char sc = -3;
    unsigned char uc = 250;
    if (mixed_multiply(sc, uc) != -750)
        return 5;

    sc = -128; // INT_MIN
    if (sc != -128) {
        return 6;
    }

    if (decrement(sc) != 127) {
        return 7;
    }

    return 0;
}
