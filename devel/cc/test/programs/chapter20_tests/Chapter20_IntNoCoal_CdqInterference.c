int target(int a, int b, int c) {
    static int i = 100;
    if (a || b) {
        return 0;
    }
    return i / c;
}
int main(void) {
    if (target(0, 0, 10) != 10) {
        return 1;
    }
    return 0;
}

