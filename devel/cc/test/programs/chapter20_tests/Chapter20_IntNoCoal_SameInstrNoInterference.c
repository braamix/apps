int target(void) {
    int a = id(2);
    int b = id(3);
    int c = id(4);
    int d = id(5);
    int e = id(6);
    check_5_ints(a, b, c, d, e, 2);
    int f = a * a;
    int g = b + b;
    int h = c - c;
    int i = d * d;
    int j = e + e;
    check_one_int(0, 0);
    check_one_int(f, 4);
    check_one_int(g, 6);
    check_one_int(h, 0);
    check_one_int(i, 25);
    check_one_int(j, 12);
    return 0;
}
int main(void) { return target(); }

