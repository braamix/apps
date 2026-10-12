double uint_to_double(unsigned int ui) {
    return (double) ui;
}

double ulong_to_double(unsigned long ul) {
    return (double) ul;
}

int main(void) {
    if (uint_to_double(1000u) != 1000.0) { return 1; }
    if (uint_to_double(4294967200u) != 4294967200.0) { return 2; }
    if (ulong_to_double(138512825844ul) != 138512825844.0) { return 3; }
    return 0;
}
