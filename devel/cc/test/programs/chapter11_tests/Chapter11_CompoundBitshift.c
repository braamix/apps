int main(void) {
    int x = 100;
    x <<= 22l;
    if (x != 419430400) {
        return 1;
    }
    if ((x >>= 4l) != 26214400) {
        return 2;
    }
    if (x != 26214400) {
        return 3;
    }
    long l = 12345l;
    if ((l <<= 33) != 106042742538240l) {
        return 4;
    }
    l = -l;
    if ((l >>= 10) != -103557365760l) {
        return 5;
    }
    return 0;
}
