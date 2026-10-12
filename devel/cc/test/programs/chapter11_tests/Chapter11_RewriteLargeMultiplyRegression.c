int check_12_ints(int start, int a, int b, int c, int d, int e, int f, int g,
                  int h, int i, int j, int k, int l);

long glob = 5l;

int main(void) {
    long should_spill = glob * 4294967307l;

    int one = glob - 4;
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
    int thirteen = glob + 8;
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
    if (should_spill != 21474836535l) {
        return -1;
    }
    return 0;
}

int check_12_ints(int a, int b, int c, int d, int e, int f, int g, int h, int i,
                  int j, int k, int l, int start) {
    int expected = 0;
    expected = start + 0;
    if (a != expected) {
        return expected;
    }
    expected = start + 1;
    if (b != expected) {
        return expected;
    }
    expected = start + 2;
    if (c != expected) {
        return expected;
    }
    expected = start + 3;
    if (d != expected) {
        return expected;
    }
    expected = start + 4;
    if (e != expected) {
        return expected;
    }
    expected = start + 5;
    if (f != expected) {
        return expected;
    }
    expected = start + 6;
    if (g != expected) {
        return expected;
    }
    expected = start + 7;
    if (h != expected) {
        return expected;
    }
    expected = start + 8;
    if (i != expected) {
        return expected;
    }
    expected = start + 9;
    if (j != expected) {
        return expected;
    }
    expected = start + 10;
    if (k != expected) {
        return expected;
    }
    expected = start + 11;
    if (l != expected) {
        return expected;
    }
    return 0;
}
