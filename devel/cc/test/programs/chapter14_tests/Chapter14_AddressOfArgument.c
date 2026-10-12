int addr_of_arg(int a) {
    int *ptr = &a;
    *ptr = 10;
    return a;
}

int main(void) {
    int result = addr_of_arg(-20);
    if (result != 10) {
        return 1;
    }

    int var = 100;
    result = addr_of_arg(var);
    if (result != 10) {
        return 2;
    }
    if (var != 100) {
        return 3;
    }
    return 0;
}
