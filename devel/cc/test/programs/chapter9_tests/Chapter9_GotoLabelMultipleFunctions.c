int foo(void) {
    goto label;
    return 0;
    label:
        return 5;
}

int main(void) {
    goto label;
    return 0;
    label:
        return foo();
}
