int glob1 = 1;
int glob2 = 2;
int glob3 = 3;
int glob4 = 4;
int glob5 = 5;
int callee(int a, int b, int c, int d, int e) {
    glob1 = -a;
    glob2 = -b;
    glob3 = -c;
    glob4 = -d;
    glob5 = -e;
    check_5_ints(1, 2, 3, 4, 5, 1);
    return 0;
}
int target(void) {
    int a = 99 * glob1;
    int b = 200 / glob2;
    int c = glob3 ? 104 - glob3 : 0;
    int d = c + (glob4 || glob1);
    int e = 108 - glob5;
    callee(a, b, c, d, e);
    check_one_int(glob1, -99);
    check_one_int(glob2, -100);
    check_one_int(glob3, -101);
    check_one_int(glob4, -102);
    check_one_int(glob5, -103);
    int f = a - 100;
    int g = b - 100;
    int h = c - 100;
    int i = d - 100;
    int j = e - 100;
    glob1 = f;
    glob2 = g;
    glob3 = h;
    glob4 = i;
    glob5 = j;
    check_one_int(a, 99);
    check_one_int(b, 100);
    check_one_int(c, 101);
    check_one_int(d, 102);
    check_one_int(e, 103);
    check_5_ints(glob1, glob2, glob3, glob4, glob5, -1);
    return 0;
}
int main(void) { return target(); }

