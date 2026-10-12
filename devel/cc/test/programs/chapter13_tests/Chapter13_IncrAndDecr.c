int main(void) {
    static double d = 0.75;
    if (d++ != 0.75) { return 1; }
    if (d != 1.75) { return 2; }
    d = -100.25;
    if (++d != -99.25) { return 3; }
    if (d != -99.25) { return 4; }
    if (d-- != -99.25) { return 5; }
    if (d != -100.25) { return 6; }
    if (--d != -101.25) { return 7; }
    if (d != -101.25) { return 8; }
    return 0;
}
