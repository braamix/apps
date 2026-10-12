int test(int i, int b, int c) {
    i += 2147483648l;
    if (i != 2147483628) {
        return 1;
    }
    if (b != 2147483647) {
        return 2;
    }
    b /= -34359738367l;
    if (b) {
        return 3;
    }
    if (i != 2147483628) {
        return 4;
    }
    if (c != -5000000) {
        return 5;
    }
    c *= 10000l;
    if (c != 1539607552) {
        return 6;
    }
    return 0;
}

int main(void) {
    return test(-20, 2147483647, -5000000);
}
