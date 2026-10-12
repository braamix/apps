double glob = 3.0;
double callee(void) { return 10.0; }
int main(void) {
    double d = glob;
    double x = callee();
    return (d + x == 13.0) ? 0 : 1;
}

