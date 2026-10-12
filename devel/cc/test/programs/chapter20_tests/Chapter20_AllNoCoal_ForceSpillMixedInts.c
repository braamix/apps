unsigned int glob_three = 3;
long glob_11 = 11l;
double glob_12 = 12.0;
long glob_23 = 23l;
double glob_24 = 24.0;
int target(void) {
    long should_spill = glob_three + 3;
    unsigned int one = glob_three - 2;
    long two = one + one;
    unsigned long three = 2 + one;
    char four = two * two;
    signed char five = 6 - one;
    int six = two * three;
    unsigned char seven = one + 6;
    long eight = two * 4;
    unsigned long nine = three * three;
    char ten = four + six;
    long* eleven = &glob_11;
    double* twelve = &glob_12;
    check_12_vals(one, two, three, four, five, six, seven, eight, nine, ten,
                  eleven, twelve, 1);
    unsigned int thirteen = 10 + glob_three;
    long fourteen = thirteen + 1;
    unsigned long fifteen = 28 - thirteen;
    char sixteen = fourteen + 2;
    signed char seventeen = 4 + thirteen;
    int eighteen = 32 - fourteen;
    unsigned char nineteen = 35 - sixteen;
    unsigned int twenty = fifteen + 5;
    long twenty_one = thirteen * 2 - 5;
    unsigned long twenty_two = fifteen + 7;
    long* twenty_three = &glob_23;
    double* twenty_four = &glob_24;
    check_12_vals(thirteen, fourteen, fifteen, sixteen, seventeen, eighteen,
                  nineteen, twenty, twenty_one, twenty_two, twenty_three,
                  twenty_four, 13);
    if (should_spill != 6) {
        return -1;
    }
    return 0;
}
int main(void) { return target(); }

