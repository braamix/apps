int check_null_ptr_cast(void) {
    static long *long_ptr = 0;
    double *dbl_ptr = (double *)long_ptr;
    unsigned int *int_ptr = (unsigned int *)long_ptr;
    int **ptr_ptr = (int **)long_ptr;

    if (long_ptr) {
        return 1;
    }
    if (dbl_ptr) {
        return 2;
    }
    if (int_ptr) {
        return 3;
    }
    if (ptr_ptr) {
        return 4;
    }
    return 0;
}

int check_round_trip(void) {
    long l = -1;
    long *long_ptr = &l;
    double *dbl_ptr = (double *)long_ptr;
    long *other_long_ptr = (long *)dbl_ptr;
    if (*other_long_ptr != -1) {
        return 5;
    }
    return 0;
}

int main(void)
{
    int result = check_null_ptr_cast();

    if (result) {
        return result;
    }

    result = check_round_trip();
    return result;
}
