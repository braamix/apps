int lt(double d, long l) {
    return d < l;
}

double tern_flag(double flag) {
    return (double) (flag ? 30 : 10ul);
}

double tern_result(int flag) {
    return flag ? 5.0 : 1000ul;
}
int ten = 10;
int multiply(void) {
    int i = 10.75 * ten;
    return i == 107;
}

int main(void) {
    if (lt(10.0, 5l)) { return 1; }
    if (tern_flag(20.0) != 30.0) { return 2; }
    if (tern_flag(0.0) != 10.0) { return 3; }
    if (tern_result(1) != 5.0) { return 4; }
    if (tern_result(0) != 1000.0) { return 5; }
    if (!multiply()) { return 6; }
    return 0;
}
