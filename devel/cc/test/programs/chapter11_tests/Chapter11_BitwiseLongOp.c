int main(void) {
    long l1 = 1095233372415l;  // 0xff_00ff_00ff
    long l2 = -4294967296;  // -2^32

    if ((l1 & l2) != 1095216660480l) {
        return 1;
    }
    if ((l1 | l2) != -4278255361) {
        return 2;
    }
    if ((l1 ^ l2) != -1099494915841l) {
        return 3;
    }
    if ((-1l & 34359738368l) != 34359738368l) {
        return 4;
    }
    if ((0l | 34359738368l) != 34359738368l) {
        return 5;
    }
    if ((34359738368l ^ 137438953472l) != 171798691840l) {
        return 6;
    }
    long l = 274877906943l;  // 0x3f_ffff_ffff = 2^38 - 1
    int i = -1073741824;
    int i2 = -1;
    if ((i & l) != 273804165120l) {
        return 7;
    }
    if ((l | i) != -1) {
        return 8;
    }
    if ((l ^ i) != -273804165121l) {
        return 9;
    }
    if ((i2 ^ 274877906943l) != ~274877906943l) {
        return 10;
    }
    return 0;
}
