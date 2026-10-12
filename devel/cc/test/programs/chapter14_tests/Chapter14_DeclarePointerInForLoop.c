int main(void) {
    int x = 10;
    for (int *i = &x; i != 0; ) {
        *i = 5;
        i = 0;
    }
    return x;
}
