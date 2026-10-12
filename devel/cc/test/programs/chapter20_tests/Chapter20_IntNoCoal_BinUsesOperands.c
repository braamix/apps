int src_test(int arg) {
    int x = 5 + arg;
    check_one_int(x, 6);
    return 0;
}
int glob = 1;
int glob2;
int flag = 1;
int dst_test(void) {
    int a = id(100);
    if (flag) {
        glob2 = a + glob;
        a = a - 1;
    }
    check_one_int(a, 99);
    check_one_int(glob2, 101);
    return 0;
}
int main(void) {
    src_test(1);
    dst_test();
    return 0;
}

