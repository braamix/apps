int main(void) {
    int coalesce_into_eax = id(10);
    int sum = coalesce_into_eax + 4;
    if (sum != 14) {
        return -1;
    }
    int rem = coalesce_into_eax % 10;
    check_one_int(rem, 0);
    return 0;
}

