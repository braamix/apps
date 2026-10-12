double glob1 = 1.;
double glob2 = 2.;
double glob10 = 10.;
int target(double one, double two, double three, double four, double five,
           double six, double seven, double eight) {
    double nine = (glob1 + glob2) * 3.;
    double ten = (glob2 + 3.) * 2.;
    double eleven = (glob10 + 1.) * glob1;
    double twelve = (glob1 + glob2) * 4.;
    double thirteen = (glob2 * 3.) + 7.;
    double fourteen = glob2 * 6. + 2.;
    check_14_doubles(one, two, three, four, five, six, seven, eight, nine, ten,
                     eleven, twelve, thirteen, fourteen, 1.);
    return 0;
}
int main(void) { return target(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0); }

