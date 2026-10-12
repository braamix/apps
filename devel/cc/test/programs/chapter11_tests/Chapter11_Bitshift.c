int main(void) {
    long l = 137438953472l; // 2^37
    int shiftcount = 2;

    if (l >> shiftcount != 34359738368l /* 2 ^ 35 */) {
        return 1;
    }
    if (l << shiftcount != 549755813888 /* 2 ^ 39 */) {
        return 2;
    }
    if (l << 2 != 549755813888 /* 2 ^ 39 */) {
        return 3;
    }
    if ((40l << 40) !=  43980465111040l) {
        return 4;
    }
    long long_shiftcount = 3l;
    int i_neighbor1 = 0;
    int i = -2147483645; // -2^31 + 3
    int i_neighbor2 = 0;
    if (i >> long_shiftcount != -268435456) {
        return 5;
    }
    i = -1;
    if (i >> 10l != -1) {
        return 6;
    }
    if (i_neighbor1) {
        return 7;
    }
    if (i_neighbor2) {
        return 8;
    }
    return 0;
}
