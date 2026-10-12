long add(int a, int b);

int main(void) {
    long a = add(2147483645, 2147483645);
    if (a != 4294967290l) {
        return 1;
    }
    return 0;
}

long add(int a, int b) {
    return (long) a + (long) b;
}
