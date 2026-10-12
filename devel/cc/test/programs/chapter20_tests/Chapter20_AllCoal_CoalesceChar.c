char glob_a;
char glob_b;
char glob_c;
char glob_d;
char glob_e;
char glob_f;
int glob = 0;
void set_glob(void) {
    glob = 10;
}
int target(char a, char b, char c, char d, char e, char f) {
    glob_a = a;
    glob_b = b;
    glob_c = c;
    glob_d = d;
    glob_e = e;
    glob_f = f;
    set_glob();
    check_six_chars(glob_a, glob_b, glob_c, glob_d, glob_e, glob_f, 1);
    check_one_int(glob, 10);
    return 0;
}
int main(void) { return target(1, 2, 3, 4, 5, 6); }

