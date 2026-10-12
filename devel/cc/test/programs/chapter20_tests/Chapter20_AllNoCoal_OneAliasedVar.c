void increment(int *ptr) {
    *ptr = *ptr + 1;
    return;
}
double deref(double *ptr) {
    return *ptr;
}
int target(int one, int two, int three, double one_d) {
    int *ptr = &one;
    double *d_ptr = &one_d;
    check_one_double(deref(d_ptr), 1.0);
    increment(ptr);
    long five = two + three;
    check_one_int(one, 2);
    check_one_long(five, 5l);
    return 0;
}
int main(void) { return target(1, 2, 3, 1.0); }

