int main(void) {
    int a = 10;
    int f(int a);
    return f(a);
}

int f(int a) {
    return a * 2;
}
