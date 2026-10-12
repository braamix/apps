int main(void) {
    long l = -34359738368l; // -2^35
    int i = -10;
    l -= i;
    if (l != -34359738358l) {
        return 1;
    }
    return 0;
}
