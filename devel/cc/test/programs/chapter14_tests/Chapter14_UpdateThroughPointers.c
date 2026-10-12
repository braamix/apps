int main(void) {
    unsigned int i = 2185232384u;
    signed long l = 1099511627000l;
    double d = 1.0;

    unsigned *i_ptr = &i;
    long *l_ptr = &l;
    double *d_ptr = &d;

    *i_ptr = 10;
    *l_ptr = -20;
    *d_ptr = 30.1;

    if (i != 10) {
        return 1;
    }
    if (l != -20) {
        return 2;
    }
    if (d != 30.1) {
        return 3;
    }
    return 0;
}
