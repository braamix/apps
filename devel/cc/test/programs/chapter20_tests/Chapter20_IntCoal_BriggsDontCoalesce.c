int glob = 5;
int update_glob(void) {
    glob = glob + 1;
    return 0;
}
int target(void) {
    int z = glob + 10;
    int a;
    int one = glob - 4;
    int two = glob / 2;
    int three = -glob + 8;
    int four = glob - 1;
    update_glob();
    if (glob) {
       a = 1 - z;
    } else {
        a = 5;
    }
    check_one_int(one, 1);
    check_one_int(two, 2);
    check_one_int(three, 3);
    check_one_int(four, 4);
    check_one_int(a, -14);
    check_one_int(glob, 6);
    return 0;
}
int main(void) { return target(); }

