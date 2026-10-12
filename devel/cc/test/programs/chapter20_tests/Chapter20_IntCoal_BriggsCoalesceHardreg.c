int flag = 1;
int target(void) {
    int coalesce_into_eax = 0;
    if (flag) {
        coalesce_into_eax = id(10);
    }
    int high_degree = 2 * coalesce_into_eax;
    if (coalesce_into_eax != 10) {
        return -1;
    }
    int twelve = 32 - high_degree;
    int eleven = 23 - twelve;
    int ten = 21 - eleven;
    int nine = 19 - ten;
    int eight = 17 - nine;
    int seven = 15 - eight;
    int six = 13 - seven;
    int five = 11 - six;
    int four = 24 - high_degree;
    int three = 23 - high_degree;
    int two = 22 - high_degree;
    int one = 21 - high_degree;
    check_5_ints(one, two, three, four, five, 1);
    return 0;
}
int main(void) { return target(); }

