int target(int one, int two) {
    int three = one + two;
    int four = two * two;
    int five = three + two;
    return check_5_ints(one, two, three, four, five, 1);
}
int main(void) { return target(1, 2); }

