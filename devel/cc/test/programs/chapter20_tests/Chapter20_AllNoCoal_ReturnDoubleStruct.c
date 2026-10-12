struct s { double d1; double d2; };
int global_one = 1;
struct s return_struct(void) {
    int one = 2 - global_one;
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
    if (one == 1 && two == 2 && three == 3 && four == 4 && five == 5 &&
        six == 6 && seven == 7 && eight == 8 && nine == 9 && ten == 10 &&
        eleven == 11 && twelve == 12) {
        struct s retval = {0.0, 200.0};
        return retval;
    } else {
        struct s retval = {1.0, -1.0};
        return retval;
    }
}
int target(void) {
    struct s retval = return_struct();
    if (retval.d1 != 0.0) exit(-1);
    if (retval.d2 != 200.0) exit(-1);
    return 0;
}
int main(void) { return target(); }

