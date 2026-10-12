int main(void) {
    int a = id(1);
    int b = a + a;
    check_one_int(-1, -1);
    check_one_int(a, 1);
    check_one_int(b, 2);
    int c = id(3);
    int d = c - c;
    check_one_int(0, 0);
    check_one_int(c, 3);
    check_one_int(d, 0);
    int x = id(4);
    int y = x * x;
    check_one_int(-1, -1);
    check_one_int(x, 4);
    check_one_int(y, 16);
    return 0;
}

