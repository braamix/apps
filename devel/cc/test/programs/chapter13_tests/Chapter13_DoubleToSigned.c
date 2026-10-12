int double_to_int(double d) {
    return (int) d;
}

long double_to_long(double d) {
    return (long) d;
}

int main(void) {
    long l = double_to_long(2148429099.3);
    if (l != 2148429099l) { return 1; }
    int i = double_to_int(-200000.9999);
    if (i != -200000) { return 2; }
    return 0;
}
