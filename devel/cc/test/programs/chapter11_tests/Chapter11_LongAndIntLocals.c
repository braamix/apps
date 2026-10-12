int main(void) {
    long a = 8589934592l; // this number is outside the range of int
    int b = -1;
    long c = -8589934592l; // also outside the range of int
    int d = 10;

    if (a != 8589934592l) {
        return 1;
    }
    if (b != -1){
        return 2;
    }
    if (c != -8589934592l) {
        return 3;
    }
    if (d != 10) {
        return 4;
    }

    a = -a;
    b = b - 1;
    c = c + 8589934594l;
    d = d + 10;

    if (a != -8589934592l) {
        return 5;
    }
    if (b != -2) {
        return 6;
    }
    if (c != 2) {
        return 7;
    }
    if (d != 20) {
        return 8;
    }
    return 0;
}
