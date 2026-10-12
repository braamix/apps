int uint_to_int(unsigned int ui, int expected) {
    return (int) ui == expected;
}

int int_to_uint(int i, unsigned int expected) {
    return (unsigned int) i == expected;
}

int ulong_to_long(unsigned long ul, signed long expected) {
    return (signed long) ul == expected;
}

int long_to_ulong(long l, unsigned long expected) {
    return (unsigned long) l == expected;
}

int main(void) {

    if (!int_to_uint(10, 10u)) {
        return 1;
    }

    if (!uint_to_int(10u, 10)) {
        return 2;
    }

    if (!long_to_ulong(-1000l, 18446744073709550616ul)) {
        return 3;
    }

    if (!ulong_to_long(18446744073709550616ul, -1000l)) {
        return 4;
    }

    return 0;
}
