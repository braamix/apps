int main(void) {
    double d = 1000.5;
    d += 1000;
    if (d != 2000.5) { return 1; }
    unsigned long ul = 100000000000ul;
    ul -= 1.0e10;
    if (ul != 90000000000ul) { return 2; }
    int i = 10;
    i += 0.99999;
    if (i != 10) { return 3; }
    return 0;
}
