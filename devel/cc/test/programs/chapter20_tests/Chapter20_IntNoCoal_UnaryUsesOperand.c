int glob = 1;
int target(void) {
    int a = 0;
    if (id(1)) {
        a = id(100);
    }
    if (id(1)) {
        glob = a + glob;
        a = -a;
    }
    check_one_int(a, -100);
    check_one_int(glob, 101);
    return 0;
}
int main(void) { return target(); }

