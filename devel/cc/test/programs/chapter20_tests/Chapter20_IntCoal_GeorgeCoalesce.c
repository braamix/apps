int glob = 1;
int increment_glob(void) {
    glob = glob + 1;
    return 0;
}
int target(int a, int b, int c, int d, int e, int f) {
    if (a != 1) return 1;
    if (b != 2) return 2;
    if (c != 3) return 3;
    if (d != 4) return 4;
    if (e != 5) return 5;
    if (f != 6) return 6;
    int one = glob * 1;
    int two = glob * 2;
    int three = glob * 3;
    int four = glob * 4;
    if (one != 1) return 7;
    if (two != 2) return 8;
    if (three != 3) return 9;
    if (four != 4) return 10;
    increment_glob();
    int five = 4 + one;
    int six = 4 + two;
    int seven = 4 + three;
    int eight = 4 + four;
    if (five != 5) return 11;
    if (six != 6) return 12;
    if (seven != 7) return 13;
    if (eight != 8) return 14;
    increment_glob();
    int nine = 14 - five;
    int ten = 16 - six;
    int eleven = 18 - seven;
    int twelve = 20 - eight;
    increment_glob();
    if (nine != 9) return 15;
    if (ten != 10) return 16;
    if (eleven != 11) return 17;
    if (twelve != 12) return 18;
    int s = glob - 3;
    int t = glob - 2;
    int u = glob - 1;
    int v = glob * 2 - 4;
    int w = glob + 1;
    check_5_ints(s, t, u, v, w, 1);
    return check_one_int(glob, 4);
}
int main(void) { return target(1, 2, 3, 4, 5, 6); }

