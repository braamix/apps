struct s { long l; double d; };
long glob = 100;
double glob_d = 200.0;
long x = 0;
double y = 0;
int callee(struct s s1, long a, double b) {
    if (s1.l != -50) exit(-1);
    if (s1.d != -40.0) exit(-1);
    if (a != 101) exit(-1);
    if (b != 202.0) exit(-1);
    return 0;
}
int main(void) {
    long a = glob + 1;
    double b = glob_d + 2.0;
    struct s s1 = {-50, -40.0};
    x = a * glob;
    y = b * glob_d;
    callee(s1, a, b);
    check_one_long(x, 10100);
    check_one_double(y, 40400.0);
    return 0;
}

