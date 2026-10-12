int f(int a, int b) {
    if (a != 11) exit(-1);
    if (b != 12) exit(-1);
    return 0;
}
int glob = 10;
int x = 0;
int y = 0;
int target(void) {
    int a = glob + 1;
    int b = glob + 2;
    x = a * glob;
    y = b * glob;
    f(a, b);
    check_one_int(x, 110);
    check_one_int(y, 120);
    return 0;
}
int main(void) { return target(); }

