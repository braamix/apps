double src_test(double arg) {
    double x = 5 + arg;
    check_one_double(x, 6.0);
    return 0;
}
double glob = 1;
double glob2;
int flag = 1;
int dst_test(void) {
    double a = dbl_id(100.0);
    if (flag) {
        glob2 = a + glob;
        a = a / 2.0;
    }
    check_one_double(a, 50.0);
    check_one_double(glob2, 101.0);
    return 0;
}
int main(void) {
    src_test(1);
    dst_test();
    return 0;
}

