int main(void) {
    int x = 10;
    int *y = &x;

    if (++*y != 11) {
        return 1;
    }
    if (x != 11) {
        return 2;
    }

    if (--*y != 10) {
        return 3;
    }

    if (x != 10) {
        return 4;
    }

    if ((*y)++ != 10) {
        return 5;
    }

    if (x != 11) {
        return 6;
    }

    if ((*y)-- != 11) {
        return 7;
    }

    if (x != 10) {
        return 8;
    }

    unsigned long ul = 0;
    unsigned long *ul_ptr = &ul;
    if ((*ul_ptr)--) {
        return 9;
    }
    if (ul != 18446744073709551615UL) { // wraparound from 0 to ULONG_MAX
        return 10;
    }

    double d = 0.0;
    double *d_ptr = &d;
    if (++(*d_ptr) != 1.0) {
        return 11;
    }
    if (d != 1.0) {
        return 12;
    }

    return 0;
}
