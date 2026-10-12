int target(double x, double y) {
    return 10 - (3.0 * y + x);
}
int main(void) { return target(1.0, 2.0) == 3 ? 0 : 1; }

