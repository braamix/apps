long glob = 5l;
long glob7;
long glob8;
long glob9;
long glob10;
long glob11;
long glob12;
int target(long one, long two, long three, long four, long five, long six) {
    long seven = (glob - 2l) + four;
    long eight = (glob - 1l) * two;
    long nine = (glob - 2l) * three;
    long ten = (10l - glob) * two;
    long eleven = (glob * two) + one;
    long twelve = (glob + 1l) * two;
    glob7 = seven;
    glob8 = eight;
    glob9 = nine;
    glob10 = ten;
    glob11 = eleven;
    glob12 = twelve;
    check_12_longs(one, two, three, four, five, six, 7l, 8l, 9l, 10l, 11l, 12l,
                   1l);
    check_one_long(glob7, 7l);
    check_one_long(glob8, 8l);
    check_one_long(glob9, 9l);
    check_one_long(glob10, 10l);
    check_one_long(glob11, 11l);
    check_one_long(glob12, 12l);
    return 0;
}
int main(void) { return target(1, 2, 3, 4, 5, 6); }

