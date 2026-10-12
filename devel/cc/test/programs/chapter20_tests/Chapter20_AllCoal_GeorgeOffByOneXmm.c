double glob = 0.0;
int target(double a) {
    double one = 2.0 - a;
    double two = one + one;
    double three = 2.0 + one;
    double four = two * two;
    double five = 6.0 - one;
    double six = two * three;
    double seven = one + 6.0;
    double eight = two * 4.0;
    double nine = three * three;
    double ten = four + six;
    double eleven = 16.0 - five;
    double twelve = eleven + one;
    double thirteen = five + eight;
    double fourteen = seven * two;
    glob = one;
    check_14_doubles(1, two, three, four, five, six, seven, eight, nine, ten,
                     eleven, twelve, thirteen, fourteen, 1);
    check_one_double(glob, 1.0);
    return 0;
}
int main(void) { return target(1.0); }

