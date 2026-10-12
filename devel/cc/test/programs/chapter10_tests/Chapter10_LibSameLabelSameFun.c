static int lib_f(void) {
    goto x;
    return 0;
    x:
    return 2;
}

int f_caller(void) {
    return lib_f();
}

int f(void) {
    goto x;
    return 0;
x:
    return 1;
}

int f_caller(void);

int main(void) {
    if (f() != 1) {
        return 1;
    }
    if (f_caller() != 2) {
        return 2;
    }
    return 0;
}
