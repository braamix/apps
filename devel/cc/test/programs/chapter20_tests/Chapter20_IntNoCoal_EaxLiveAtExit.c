int glob = 10;
int glob2 = 0;
int target(void) {
    int x = glob + 1;
    glob2 = x + glob;
    return x;
}
int main(void) {
    int retval = target();
    check_one_int(retval, 11);
    check_one_int(glob2, 21);
    return 0;
}

