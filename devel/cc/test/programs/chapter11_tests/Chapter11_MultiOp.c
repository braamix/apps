int target(long a) {
    long b = a * 5l - 10l;
    if (b == 21474836440l) {
        return 1;
    }
    return 0;
}

int main(void) {
    return target(4294967290l);
}
