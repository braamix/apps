int target(void) {
    int to_spill = id(1);
    int a = id(2);
    int b = id(3);
    int c = id(4);
    int d = id(5);
    int e = id(6);
    check_one_int(to_spill, 1);
    check_5_ints(a, b, c, d, e, 2);
    check_5_ints(1 + a, 1 + b, 1 + c, 1 + d, 1 + e, 3);
    check_one_int(to_spill, 1);
    int f = id(7);
    int g = id(8);
    int h = id(9);
    int i = id(10);
    int j = id(11);
    check_5_ints(f, g, h, i, j, 7);
    check_5_ints(1 + f, 1 + g, 1 + h, 1 + i, 1 + j, 8);
    check_one_int(to_spill, 1);
    return 0;
}
int main(void) { return target(); }

