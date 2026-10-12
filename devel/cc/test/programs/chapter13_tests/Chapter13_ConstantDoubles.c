int main(void) {
    double a = 1.0;
    double b = 1.;
    double c = 1E0;
    double d = .01e+2;
    if (! (a == b && a == c && a == d) )
        return 1;
    if (a + b + c + d != 4.0)
        return 2;
    double e = .125;
    double f = 12.5e-2;
    double g = 125.E-3;
    double h = 1250000000e-10;
    if (! (e == f && e == g && e == h) )
        return 3;
    if (e + f + g + h != 0.5)
        return 4;
    return 0;
}
