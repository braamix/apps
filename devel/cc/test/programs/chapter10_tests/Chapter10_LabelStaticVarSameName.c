int main(void) {
    static int x = 5;
    goto x;
    x = 0;
x:
    return x;
}
