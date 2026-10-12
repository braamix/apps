int check_int(int converted, int expected) {
    return (converted == expected);
}

int check_long(long converted, long expected) {
    return (converted == expected);
}

int check_ulong(unsigned long converted, unsigned long expected) {
    return (converted == expected);
}

long return_extended_uint(unsigned int u) {
    return u;
}

unsigned long return_extended_int(int i) {
    return i;
}

int return_truncated_ulong(unsigned long ul) {
    return ul;
}

int extend_on_assignment(unsigned int ui, long expected) {
    long result = ui; // implicit conversion causes zero-extension
    return result == expected;
}

int main(void) {
    if (!check_int(9223372036854775813ul, 5)) { // 2^63 + 5 truncates to 5
        return 1;
    }

    if (!check_long(2147483658u, 2147483658l)) {
        return 2;
    }

    if (!check_ulong(-1, 18446744073709551615UL)) {
        return 3;
    }

    if (return_extended_uint(2147483658u) != 2147483658l) {
        return 4;
    }

    if (return_extended_int(-1) != 18446744073709551615UL) {
        return 5;
    }

    long l = return_truncated_ulong(1125902054326372ul); // 2^50 + 2^31 + 100 truncates to INT_MIN + 100
    if (l != -2147483548l) {
        return 6;
    }

    if (!extend_on_assignment(2147483658u, 2147483658l)){
        return 7;
    }

    int i = 4294967196u; // 2^32 - 100 converts to -100
    if (i != -100) {
        return 8;
    }

    return 0;
}
