void putch(int c);
int foo(int a, int b, int c, int d, int e, int f, int g, int h) {
    putch(h);
    return a + g;
}

int main(void) {
    return foo(1, 2, 3, 4, 5, 6, 7, 65);
}

