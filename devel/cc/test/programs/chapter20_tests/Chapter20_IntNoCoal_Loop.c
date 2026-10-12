int counter = 5;
int expected_a = 2;
int update_expected_a(void);
int times_two(int x);
int target(void) {
    int z;
    int a;
    int one = counter - 4;
    int two = counter / 2;
    int three = -counter + 8;
    int four = counter - 1;
    while (counter > 0) {
        if (counter == 5)
            z = 4;
        else
            z = times_two(a);
        update_expected_a();
        a = 1 - z;
        check_one_int(a, expected_a);
        counter = counter - 1;
    }
    check_one_int(one, 1);
    check_one_int(two, 2);
    check_one_int(three, 3);
    check_one_int(four, 4);
    return 0;
}
int update_expected_a(void) {
    expected_a = 1 - (2 * expected_a);
    return 0;
}
int times_two(int x) {
    return x * 2;
}
int main(void) { return target(); }

