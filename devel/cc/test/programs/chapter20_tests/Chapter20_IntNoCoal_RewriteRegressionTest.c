int glob_three = 3;
int glob_four = 4;
int target(void) {
    int imul_result = glob_three * glob_four;
    int add_result = glob_three + glob_four;
    int sub_result = glob_four - glob_three;
    int one = glob_three - 2;
    int two = one + one;
    int three = 2 + one;
    int four = two * two;
    int five = 6 - one;
    int six = two * three;
    int seven = one + 6;
    int eight = two * 4;
    int nine = three * three;
    int ten = four + six;
    int eleven = 16 - five;
    int twelve = six + six;
    check_12_ints(one, two, three, four, five, six, seven, eight, nine, ten,
                  eleven, twelve, 1);
    int thirteen = 10 + glob_three;
    int fourteen = thirteen + 1;
    int fifteen = 28 - thirteen;
    int sixteen = fourteen + 2;
    int seventeen = 4 + thirteen;
    int eighteen = 32 - fourteen;
    int nineteen = 35 - sixteen;
    int twenty = fifteen + 5;
    int twenty_one = thirteen * 2 - 5;
    int twenty_two = fifteen + 7;
    int twenty_three = 6 + seventeen;
    int twenty_four = thirteen + 11;
    check_12_ints(thirteen, fourteen, fifteen, sixteen, seventeen, eighteen,
                  nineteen, twenty, twenty_one, twenty_two, twenty_three,
                  twenty_four, 13);
    if (imul_result != 12) return 100;
    if (add_result != 7) return 101;
    if (sub_result != 1) return 102;
    return 0;
}
int main(void) { return target(); }

