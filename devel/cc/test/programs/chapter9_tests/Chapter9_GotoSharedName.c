int foo(void) {
    goto foo;
    return 0;
    foo:
        return 1;
}

int main(void) {
    return foo();
}
