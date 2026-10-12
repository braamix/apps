int main(void) {

    int i = -100;
    unsigned long ul = 281474976710000ul;
    double d = 3.5;

    int *i_ptr = &i;
    unsigned long *ul_ptr = &ul;
    double *d_ptr = &d;

    if (*i_ptr != -100) {
        return 1;
    }
    if (*ul_ptr != 281474976710000ul) {
        return 2;
    }
    if (*d_ptr != 3.5) {
        return 3;
    }

    i = 12;
    ul = 1000;
    d = -000.001;

    if (*i_ptr != 12) {
        return 4;
    }
    if (*ul_ptr != 1000) {
        return 5;
    }
    if (*d_ptr != -000.001) {
        return 6;
    }

    int i2 = 1;
    unsigned long ul2 = 200000000000000ul;
    double d2 = -33.3;

    i_ptr = &i2;
    ul_ptr = &ul2;
    d_ptr = &d2;


    if (*i_ptr != 1) {
        return 7;
    }
    if (*ul_ptr != 200000000000000ul) {
        return 8;
    }
    if (*d_ptr != -33.3) {
        return 9;
    }

    return 0;

}
