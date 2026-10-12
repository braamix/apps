int main(void) {
    unsigned long a = 8589934592ul; // this number is outside the range of int
    int b = -1;
    long c = -8589934592l; // also outside the range of int
    unsigned int d = 10u;

    if (a != 8589934592ul) {
        return 1;
    }
    if (b != -1){
        return 2;
    }
    if (c != -8589934592l) {
        return 3;
    }
    if (d != 10u) {
        return 4;
    }

    a = -a;
    b = b - 1;
    c = c + 8589934594l;
    d = d * 268435456u; // result is between INT_MAX and UINT_MAX

    if (a != 18446744065119617024ul) {
        return 5;
    }
    if (b != -2) {
        return 6;
    }
    if (c != 2) {
        return 7;
    }
    if (d != 2684354560u) {
        return 8;
    }

    return 0;
}
