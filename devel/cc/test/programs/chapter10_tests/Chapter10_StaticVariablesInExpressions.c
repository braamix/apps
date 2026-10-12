int main(void) {
    static int i = 2;
    static int j = 3;
    int cmp = i < j;

    if (!cmp)
        return 1;
    return 0;
}
