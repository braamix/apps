int int_to_ulong(int i, unsigned long expected) {
    unsigned long result = (unsigned long) i;
    return result == expected;
}

int uint_to_long(unsigned int ui, long expected) {
    long result = (long) ui;
    return result == expected;
}

int uint_to_ulong(unsigned ui, unsigned long expected){
    return (unsigned long) ui == expected;
}

int main(void) {
    if (!int_to_ulong(10, 10ul)) {
        return 1;
    }

    if (!int_to_ulong(-10, 18446744073709551606ul)) {
        return 2;
    }

    if (!uint_to_long(4294967200u, 4294967200l)) {
        return 3;
    }

    if (!uint_to_ulong(4294967200u, 4294967200ul)) {
        return 4;
    }
    if ((unsigned long) 4294967200u != 4294967200ul) {
        return 5;
    }
    return 0;
}
