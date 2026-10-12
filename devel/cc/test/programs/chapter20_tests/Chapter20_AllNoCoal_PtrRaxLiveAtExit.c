long arr[3] = {100, 200, 300};
long glob2;
long *target(void) {
    long *ptr = arr;
    glob2 = (long)ptr + 80;
    return ptr;
}
int main(void) {
    long *retval = target();
    check_one_int(retval[0], 100);
    check_one_int(retval[1], 200);
    check_one_int(retval[2], 300);
    if (glob2 == 0) {
        return -2;
    }
    return 0;
}

