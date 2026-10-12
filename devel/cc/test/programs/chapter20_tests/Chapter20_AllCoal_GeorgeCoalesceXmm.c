double glob = 4.0;
double dbl_target(double a, double b, double c, double d, double e, double f,
                  double g, double h) {
    if (a != 1.0) return 1.0;
    if (b != 2.0) return 2.0;
    if (c != 3.0) return 3.0;
    if (d != 4.0) return 4.0;
    if (e != 5.0) return 5.0;
    if (f != 6.0) return 6.0;
    if (g != 7.0) return 7.0;
    if (h != 8.0) return 8.0;
    double s = glob - 3.0;
    double t = glob - 2.0;
    double u = glob - 1.0;
    double v = glob * 2.0 - 4.0;
    double w = glob + 1.0;
    double x = glob + 2.0;
    double y = glob + 3.0;
    double z = glob * 2.0;
    double spill = w * 10.0;
    check_14_doubles(s, t, u, v, w, x, y, z, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 1.0);
    if (spill != 50.0) {
        return 9.0;
    }
    return check_one_double(glob, 4.0);
}
int main(void) {
    return (int) dbl_target(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0);
}

