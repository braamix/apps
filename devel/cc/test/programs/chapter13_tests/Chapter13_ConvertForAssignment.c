int check_args(long l, double d) {
    return l == 2 && d == -6.0;
}

double return_double(void) {
    return 100000000000ul;
}

int check_assignment(double arg) {
    int i = 0;
    i = arg;
    return i == 4;
}
int main(void) {
    if (!check_args(2.4, -6)) { return 1; }
    if (return_double() != 100000000000.0) { return 2; }
    if (!check_assignment(4.9)) { return 3; }
    double d = 100000000000ul;
    if (d != 100000000000.) { return 4; }
    return 0;
}
