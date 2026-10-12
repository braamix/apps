int glob0 = 0;
int glob1 = 1;
int glob2 = 2;
int glob3 = 3;
int glob4 = 4;
int increment_globals(void) {
    glob0 = glob0 + 1;
    glob1 = glob1 + 1;
    glob2 = glob2 + 1;
    glob3 = glob3 + 1;
    glob4 = glob4 + 1;
    return 0;
}
int validate(int zero, int one, int two, int three, int four);
int target(void) {
    int a = glob0;
    int b = glob1;
    int c = glob2;
    int d = glob3;
    int e = glob4;
    increment_globals();
    int x = a;
    if (a > b) {
        return 1;
    }
    if (b) {
        x = 100;
    }
    increment_globals();
    return validate(x, b, c, d, e);
}
int validate(int hundred, int one, int two, int three, int four) {
    if (glob0 != 2) return 2;
    if (glob1 != 3) return 3;
    if (glob2 != 4) return 4;
    if (glob3 != 5) return 5;
    if (glob4 != 6) return 6;
    if (hundred != 100) return 7;
    if (one != 1) return 8;
    if (two != 2) return 9;
    if (three != 3) return 10;
    if (four != 4) return 11;
    return 0;
}
int main(void) { return target(); }

