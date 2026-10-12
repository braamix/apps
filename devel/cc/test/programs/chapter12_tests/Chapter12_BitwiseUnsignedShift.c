int main(void) {
    unsigned int ui = -1u;  // 2^32 - 1

    if ((ui << 2l) != 4294967292) { // 2^32 - 4
        return 1;
    }

    if ((ui >> 2) != 1073741823) { // 2^30 - 1
        return 2;
    }

    static int shiftcount = 5;
    if ((1000000u >> shiftcount) != 31250) {
        return 3;
    }

    if ((1000000u << shiftcount) != 32000000) {
        return 4;
    }

    return 0;
}
