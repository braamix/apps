int main(void) {
    int sum = 0;
    if (1) {
        int a = 5;
        goto other_if;
        sum = 0;
    first_if:
        a = 5;
        sum = sum + a;
    }
    if (0) {
    other_if:;
        int a = 6;
        sum = sum + a;
        goto first_if;
        sum = 0;
    }
    return sum;
}
