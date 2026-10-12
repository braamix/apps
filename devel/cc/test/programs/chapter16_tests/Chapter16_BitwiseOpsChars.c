// make sure we perform integer promotions when performing bitwise operations on chars

int main(void) {
    unsigned char uc = 135;
    signed char c = -116; // plain char may be unsigned; keep the negative value signed
    if ((uc & c) != 132) {
        return 1;  // fail
    }

    if ((uc | c) != -113) {
        return 2;  // fail
    }

    if (((c ^ 1001u) | 360l) != 4294966637) { // 2^32 - 659
        return 3; // fail
    }

    return 0;
}
