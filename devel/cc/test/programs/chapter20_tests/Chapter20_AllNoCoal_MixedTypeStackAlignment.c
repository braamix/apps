int check_alignment(int exit_code) { return 0; }
int test1(void) {
    long a = id(1);
    unsigned long b = id(2);
    long c = id(3);
    unsigned long d = id(4);
    long e = id(5);
    unsigned long f = id(6);
    long g = id(7);
    check_alignment(-1);
    check_one_int(a, 1);
    check_one_int(b, 2);
    check_one_int(c, 3);
    check_one_int(d, 4);
    check_one_int(e, 5);
    check_one_int(f, 6);
    check_one_int(g, 7);
    return 0;
}
int test2(void) {
    char a = id(4);
    unsigned int b = id(5);
    char arr[11] = { 'a', 'b', 'c', 'd','e', 'f', 'g', 'h', 'i', 'j', 'k', };
    check_alignment(-2);
    check_one_int(a, 4);
    check_one_int(b, 5);
    for (int i = 0; i < 11; i = i + 1) {
        check_one_int(arr[i], 'a' + i);
    }
    return 0;
}
int test3(void) {
    static int *ptr;
    char a = id(4);
    unsigned char b = id(5);
    long c = id(6);
    int aliased = 10;
    ptr = &aliased;
    check_alignment(-3);
    check_one_int(a, 4);
    check_one_int(b, 5);
    check_one_int(c, 6);
    check_one_int(*ptr, 10);
    return 0;
}
int main(void) { test1(); test2(); test3(); return 0; }

