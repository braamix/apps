int global_one = 1;
double return_double(void) {
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
        return 0.0;
    } else {
        return 1.0;
    }
}
int target(void) {
    return (int) return_double();
}
int main(void) { return target(); }

