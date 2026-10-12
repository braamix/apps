double zero = 0.0;
double non_zero = 1E-15;
double one = 1.0;
double fp_zero = 0.0;

int main(void) {
    if (zero) { return 1; }
    if (fp_zero) { return 2; }
    if (non_zero) {
    } else {
        return 3;
    }
    if (0.e10) { return 4; }
    if (!non_zero) { return 4; }
    if (!(!zero)) { return 5; }
    if (!(!fp_zero)) { return 6; }
    if (!(non_zero && 1.0)) { return 8; }
    if (3.0 && zero) { return 8; }
    if (fp_zero && 1000e10) { return 9; }
    if (18446744073709551615UL && zero) { return 10; }
    if (!(non_zero && 5l)) { return 11; }
    if (!(5.0 || zero)) { return 12; }
    if (zero || fp_zero) { return 13; }
    if (!(fp_zero || 0.0001)) { return 14; }
    if (!(non_zero || 0u)) { return 15; }
    if (!(0 || 0.0000005)) { return 16; }
    return 0;
}
