long sign_extend(int i, long expected) {
    long extended = (long) i;
    return (extended == expected);
}

int main(void) {
    if (!sign_extend(10, 10l)) {
        return 1;
    }
    if (!sign_extend(-10, -10l)) {
        return 2;
    }
    long l = (long) 100;
    if (l != 100l) {
        return 3;
    }
    return 0;
}
