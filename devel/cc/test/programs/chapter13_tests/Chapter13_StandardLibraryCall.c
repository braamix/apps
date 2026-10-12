double fma(double x, double y, double z);
double ldexp(double x, int exp);

int main(void) {
    double fma_result = fma(5.0, 1e6, 4000000.0);
    double ldexp_result = ldexp(1500.0, 5);
    if (fma_result != 9000000.0) { return 1; }
    if (ldexp_result != 48000.0) { return 2; }
    return 0;
}
