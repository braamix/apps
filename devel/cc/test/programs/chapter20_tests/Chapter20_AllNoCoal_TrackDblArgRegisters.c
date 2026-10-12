int callee(double a, double b, double c) {
    if (a != 12) exit(-1);
    if (b != 13) exit(-1);
    if (c != 14) exit(-1);
    return 0;
}
double glob1; double glob2; double glob3; double glob4; double glob5;
double glob6; double glob7; double glob8; double glob9; double glob10;
double glob11;
int target(double one, double two, double three) {
    double four = three + one;
    double five = two + three;
    double six = three * two;
    double seven = 13. - six;
    double eight = four * two;
    double nine = three * three;
    double ten = five * two;
    double eleven = seven * two - three;
    double twelve = eight * four - 20.;
    double thirteen = (nine + ten) - six;
    double fourteen = eleven + 3;
    glob1 = one; glob2 = two; glob3 = three; glob4 = four; glob5 = five;
    glob6 = six; glob7 = seven; glob8 = eight; glob9 = nine; glob10 = ten;
    glob11 = eleven;
    callee(twelve, thirteen, fourteen);
    check_14_doubles(glob1, glob2, glob3, glob4, glob5, glob6, glob7, glob8,
                     glob9, glob10, glob11, 12., 13., 14., 1);
    return 0;
}
int main(void) { return target(1.0, 2.0, 3.0); }

