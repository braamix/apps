int test_sum(int a, int b, int c, long d, int e, long f, int g, int h, long i);

int main(void) {
    return test_sum(0, 0, 0, 34359738368l, 0, 34359738368l, 0, 0, 34359738368l);
}

int test_sum(int a, int b, int c, long d, int e, long f, int g, int h, long i) {
    if (d + f < 100l) {
        return 1;
    }
    if (i < 100l)
        return 2;
    return 0;
}
