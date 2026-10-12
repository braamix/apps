unsigned int double_to_uint(double d) {
    return (unsigned int) d;
}

unsigned long double_to_ulong(double d) {
    return (unsigned long) d;
}

int main(void) {
    if (double_to_uint(10.9) != 10u) { return 1; }
    if (double_to_uint(2147483750.5) != 2147483750) { return 2; }
    if (double_to_ulong(34359738368.5) != 34359738368ul) { return 3; }
    if (double_to_ulong(100000000000.0) != 100000000000ul) { return 4; }
    return 0;
}
