double int_to_double(int i) {
    return (double) i;
}

double long_to_double(long l) {
    return (double) l;
}
int main(void) {
    if (int_to_double(-100000) != -100000.0) { return 1; }
    if (long_to_double(-100000000000l) != -100000000000.0) { return 2; }
    return 0;
}
