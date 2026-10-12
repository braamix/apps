struct s1 { double d; char c; int i; };
struct s2 { unsigned long ul; double d; };
struct s3 { double d1; double d2; signed char s; };
int callee(struct s1 a, struct s2 b, char c, struct s3 in_mem) {
    if (a.d != 11.0) exit(-1);
    if (a.c != 8) exit(-1);
    if (a.i != 9) exit(-1);
    if (b.ul != 10ul) exit(-1);
    if (b.d != 12.0) exit(-1);
    if (c != 12) exit(-1);
    if (in_mem.d1 != 13.0) exit(-1);
    if (in_mem.d2 != 14.0) exit(-1);
    if (in_mem.s != 11) exit(-1);
    return 0;
}
int check_some_args(int one, long two, unsigned int three, unsigned long four,
                    char five, unsigned char six, signed char seven) {
    if (one != 1) exit(-1);
    if (two != 2l) exit(-1);
    if (three != 3u) exit(-1);
    if (four != 4ul) exit(-1);
    if (five != 5) exit(-1);
    if (six != 6) exit(-1);
    if (seven != 7) exit(-1);
    return 0;
}
int glob1; long glob2; unsigned int glob3; unsigned long glob4;
char glob5; unsigned char glob6; signed char glob7; long glob8;
double glob1_d; double glob2_d; double glob3_d; double glob4_d; double glob5_d;
double glob6_d; double glob7_d; double glob8_d; double glob9_d; double glob10_d;
double glob11_d; double glob12_d; double glob13_d;
int target(int one, int two, int three, double one_d, double two_d) {
    long four = two + 2;
    char five = three + two;
    int six = 12 - one - two - three;
    unsigned int seven = 13 - six;
    unsigned char eight = four * two;
    unsigned long nine = three * three;
    signed long ten = six + four;
    signed char eleven = six * two - one;
    int twelve = six * two;
    double three_d = one_d + two_d;
    double four_d = three_d + one_d;
    double five_d = two_d + three_d;
    double six_d = three_d * two_d;
    double seven_d = 13. - six_d;
    double eight_d = four_d * two_d;
    double nine_d = three_d * three_d;
    double ten_d = five_d * two_d;
    double eleven_d = seven_d * two_d - three_d;
    double twelve_d = eight_d * four_d - 20.;
    double thirteen_d = (nine_d + ten_d) - six_d;
    double fourteen_d = eleven_d + 3;
    glob1 = one; glob2 = two; glob3 = three; glob4 = four; glob5 = five;
    glob6 = six; glob7 = seven;
    glob1_d = one_d; glob2_d = two_d; glob3_d = three_d; glob4_d = four_d;
    glob5_d = five_d; glob6_d = six_d; glob7_d = seven_d; glob8_d = eight_d;
    glob9_d = nine_d; glob10_d = ten_d;
    struct s1 arg1 = {eleven_d, eight, nine};
    struct s2 arg2 = {ten, twelve_d};
    struct s3 in_mem = {thirteen_d, fourteen_d, eleven};
    callee(arg1, arg2, twelve, in_mem);
    check_some_args(glob1, glob2, glob3, glob4, glob5, glob6, glob7);
    check_14_doubles(glob1_d, glob2_d, glob3_d, glob4_d, glob5_d, glob6_d,
                     glob7_d, glob8_d, glob9_d, glob10_d, 11.0, 12.0, 13., 14.,
                     1);
    return 0;
}
int main(void) { return target(1, 2, 3, 1.0, 2.0); }

