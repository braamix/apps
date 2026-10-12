int flag = 0;
int result = 0;
int glob0;
int glob1;
int glob2;
int glob3;
int glob4;
int set_globals(int start) {
    glob0 = start;
    glob1 = start + 1;
    glob2 = start + 2;
    glob3 = start + 3;
    glob4 = start + 4;
    return 0;
}
int target(void) {
    set_globals(0);
    int zero = glob0;
    int one = glob1;
    int two = glob2;
    int three = glob3;
    int four = glob4;
    set_globals(5);
    int five = glob0;
    int six = glob1;
    int seven = glob2;
    int eight = glob3;
    int nine = glob4;
    check_5_ints(zero, one, two, three, four, 0);
    check_5_ints(five, six, seven, eight, nine, 5);
    set_globals(10);
    int ten = glob0;
    int eleven = glob1;
    int twelve = glob2;
    int thirteen = glob3;
    int fourteen = glob4;
    check_5_ints(zero, one, two, three, four, 0);
    check_5_ints(ten, eleven, twelve, thirteen, fourteen, 10);
    check_5_ints(ten, eleven, twelve, thirteen, fourteen, 10);
    check_5_ints(ten, eleven, twelve, thirteen, fourteen, 10);
    check_5_ints(ten, eleven, twelve, thirteen, fourteen, 10);
    check_5_ints(zero - 3, one - 3, two - 3, three - 3, four - 3, -3);
    return 0;
}
int main(void) { return target(); }

