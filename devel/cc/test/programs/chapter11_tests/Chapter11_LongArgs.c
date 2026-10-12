int test_sum(long a, long b, int c, int d, int e, int f, int g, int h, long i) {
    if (a + b < 100l) {
        return 1;
    }
    if (i < 100l)
        return 2;
    return 0;
}

int main(void) {
    return test_sum(34359738368l, 34359738368l, 0, 0, 0, 0, 0, 0, 34359738368l);
}
