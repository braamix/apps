int glob1;
int glob2;
int glob3;
int glob4;
int glob5;
int glob6;
int glob7;
int glob8;
int glob9;
int callee(int a, int b, int c) {
    if (a != 10) exit(-1);
    if (b != 11) exit(-1);
    if (c != 12) exit(-1);
    return 0;
}
int target(int one, int two, int three) {
    int four = two + 2;
    int five = three + two;
    int six = 12 - one - two - three;
    int seven = 13 - six;
    int eight = four * two;
    int nine = three * three;
    int ten = six + four;
    int eleven = six * two - one;
    int twelve = six * two;
    glob1 = one;
    glob2 = two;
    glob3 = three;
    glob4 = four;
    glob5 = five;
    glob6 = six;
    glob7 = seven;
    glob8 = eight;
    glob9 = nine;
    callee(ten, eleven, twelve);
    check_12_ints(glob1, glob2, glob3, glob4, glob5, glob6, glob7, glob8, glob9,
                  ten, eleven, twelve, 1);
    return 0;
}
int main(void) { return target(1, 2, 3); }

