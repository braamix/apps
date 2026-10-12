int use_dbls(double a, double b) {
    if (a != 11.0) exit(-1);
    if (b != 12.0) exit(-1);
    return 0;
}
double glob = 10.0;
double x = 0.0;
double y = 0.0;
int target(void) {
    double a = glob + 1.0;
    double b = glob + 2.0;
    x = a * glob;
    y = b * glob;
    use_dbls(a, b);
    check_one_double(x, 110.0);
    check_one_double(y, 120.0);
    return 0;
}
int main(void) { return target(); }

