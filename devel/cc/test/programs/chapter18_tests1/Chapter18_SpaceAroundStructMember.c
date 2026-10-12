struct s { int a; };
int main(void) {
    struct s foo;
    foo .a = 10;
    int b = foo .a;
    return foo . a == b;
}
