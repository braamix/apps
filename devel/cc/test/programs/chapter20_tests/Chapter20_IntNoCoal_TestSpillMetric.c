int target(void) {
    int a = id(1);
    int b = id(2);
    int c = id(10);
    int d = id(3);
    int e = id(4);
    int f = id(5);
    check_one_int(c, 10);
    check_5_ints(a, b, d, e, f, 1);
    check_5_ints(a + 3, b + 3, d + 3, e + 3, f + 3, 4);
    check_one_int(a * 2, 2);
    check_one_int(b * 2, 4);
    check_one_int(d * 2, 6);
    check_one_int(e * 2, 8);
    check_one_int(f * 2, 10);
    return 0;
}
int main(void) { return target(); }

