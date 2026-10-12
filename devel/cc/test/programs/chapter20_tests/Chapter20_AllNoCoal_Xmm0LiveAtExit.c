double glob = 10.0;
double glob2 = 0.0;
double target(void) {
    double x = glob + 1.0;
    glob2 = x + glob;
    return x;
}
int main(void) {
    double retval = target();
    check_one_double(retval, 11.0);
    check_one_double(glob2, 21.0);
    return 0;
}

