int f(int a, int b, int c, int d);

int main(void) {
    return f(10, 2, 100, 4);
}

int f(int a, int b, int c, int d) {
    int x = a / b;
    if (a == 10 && b == 2 && c == 100 && d == 4 && x == 5)
        return 1;
    return 0;
}
