int i = 8589934592l; // 2^33
long j = 123456;

int main(void) {
    if (i != 0) {
        return 1;
    }
    if (j != 123456l) {
        return 2;
    }
    return 0;
}
