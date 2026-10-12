int glob = 0;
int target(int a) {
    int one = 2 - a;
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
    int twelve = eleven + one;
    glob = one;
    check_12_ints(1, two, three, four, five, six, seven, eight, nine, ten,
                  eleven, twelve, 1);
    check_one_int(glob, 1);
    return 0;
}
int main(void) { return target(1); }

