int main(void) {
    long x = -1099511627774l;
    if (x++ != -1099511627774l) {
        return 1;
    }
    if (x != -1099511627773l) {
        return 2;
    }
    if (--x != -1099511627774l) {
        return 3;
    }
    if (x != -1099511627774l) {
        return 4;
    }
    return 0;
}
