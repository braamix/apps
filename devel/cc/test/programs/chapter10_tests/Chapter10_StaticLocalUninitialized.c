int foo(void) {
    static int x;
    x = x + 1;
    return x;
}

int main(void) {
    int ret;
    for (int i = 0; i < 4; i = i + 1)
        ret = foo();
    return ret;
}
