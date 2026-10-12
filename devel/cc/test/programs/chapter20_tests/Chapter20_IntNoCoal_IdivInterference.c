int glob = 3;
int target(void) {
    int dividend = glob * 16;
    int quotient = dividend / 4;
    glob = dividend;
    check_one_int(quotient, 12);
    return 0;
}
int main(void) {
    target();
    check_one_int(glob, 48);
    return 0;
}

