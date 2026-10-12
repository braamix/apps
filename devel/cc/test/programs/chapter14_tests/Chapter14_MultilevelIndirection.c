int main(void) {

    double d = 10.0;
    double *d_ptr = &d;
    double **d_ptr_ptr = &d_ptr;
    double ***d_ptr_ptr_ptr = &d_ptr_ptr;

    if (d != 10.0) {
        return 1;
    }
    if (*d_ptr != 10.0) {
        return 2;
    }

    if (**d_ptr_ptr != 10.0) {
        return 3;
    }

    if (***d_ptr_ptr_ptr != 10.0) {
        return 4;
    }

    if (&d != d_ptr) {
        return 5;
    }
    if (*d_ptr_ptr != d_ptr) {
        return 6;
    }
    if (**d_ptr_ptr_ptr != d_ptr) {
        return 7;
    }

    ***d_ptr_ptr_ptr = 5.0;
    if (d != 5.0) {
        return 8;
    }
    if (*d_ptr != 5.0) {
        return 9;
    }
    if (**d_ptr_ptr != 5.0) {
        return 10;
    }

    if (***d_ptr_ptr_ptr != 5.0) {
        return 11;
    }

    double d2 = 1.0;

    double *d2_ptr = &d2;
    double *d2_ptr2 = d2_ptr;

    double **d2_ptr_ptr = &d2_ptr;

    *d_ptr_ptr_ptr = d2_ptr_ptr;

    if (**d_ptr_ptr_ptr != d2_ptr) {
        return 12;
    }

    if (***d_ptr_ptr_ptr != 1.0) {
        return 13;
    }

    if (d2_ptr_ptr == &d2_ptr2)
        return 14;

    d2_ptr = d_ptr;

    if (**d_ptr_ptr_ptr != d_ptr) {
        return 15;
    }


    if (*d2_ptr_ptr != d_ptr) {
        return 16;
    }

    if (**d_ptr_ptr_ptr == d2_ptr2) {
        return 17;
    }

    if (***d_ptr_ptr_ptr != 5.0) {
        return 18;
    }

    return 0;
}
