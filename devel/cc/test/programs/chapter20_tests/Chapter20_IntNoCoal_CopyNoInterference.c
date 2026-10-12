int glob0 = 0;
int glob1 = 1;
int glob2 = 2;
int glob3 = 3;
int glob4 = 4;
int glob5 = 5;
int reset_globals(void) {
    glob0 = 0; glob1 = 0; glob2 = 0; glob3 = 0; glob4 = 0; glob5 = 0;
    return 0;
}
int flag = 1;
int target(void) {
    int a = glob0;
    int b = glob1;
    int c = glob2;
    int d = glob3;
    int e = glob4;
    int f;
    int g;
    int h;
    int i;
    int j;
    if (flag) {
        reset_globals();
        f = a; check_one_int(a, 0);
        g = b; check_one_int(b, 1);
        h = c; check_one_int(c, 2);
        i = d; check_one_int(d, 3);
        j = e; check_one_int(e, 4);
    } else {
        e = 0; f = 0; g = 0; h = 0; i = 0; j = 0;
    }
    check_one_int(f, 0);
    check_one_int(g, 1);
    check_one_int(h, 2);
    check_one_int(i, 3);
    check_one_int(j, 4);
    check_one_int(glob0, 0);
    check_one_int(glob1, 0);
    check_one_int(glob2, 0);
    check_one_int(glob3, 0);
    check_one_int(glob4, 0);
    return 0;
}
int main(void) { return target(); }

