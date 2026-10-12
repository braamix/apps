int bar(void) {
    return 9;
}

int foo(void) {
    return 2 * bar();
}

int main(void) {
    return foo() + bar() / 3;
}
