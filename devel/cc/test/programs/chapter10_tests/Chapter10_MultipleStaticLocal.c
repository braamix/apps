int foo(void) {
    static int a = 3;
    a = a * 2;
    return a;
}

int bar(void) {
    static int a = 4;
    a = a + 1;
    return a;
}

int main(void) {
    return foo() + bar() + foo() + bar();
}
