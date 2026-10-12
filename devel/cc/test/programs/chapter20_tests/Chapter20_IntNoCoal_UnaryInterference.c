int main(void) {
    int a = id(100);
    int b = -a;
    check_one_int(-1, -1);
    check_one_int(a, 100);
    check_one_int(b, -100);
    int c = id(200);
    int d = ~c;
    check_one_int(0, 0);
    check_one_int(c, 200);
    check_one_int(d, -201);
    return 0;
}

