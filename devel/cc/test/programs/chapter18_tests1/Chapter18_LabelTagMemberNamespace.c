struct x { int x; };
int main(void) {
    struct x x = {10};
    goto x;
    return 0;
x:
    return x.x;
}
