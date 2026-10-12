static int lib_my_fun(void);

int call_static_my_fun(void) {
    return lib_my_fun();
}

int call_static_my_fun_2(void) {
    return lib_my_fun();
}

static int lib_my_fun(void);

int lib_my_fun(void) {
    static int i = 0;
    i = i + 1;
    return i;
}

extern int my_fun(void);
int call_static_my_fun(void);
int call_static_my_fun_2(void);

int main(void) {
    if (call_static_my_fun() != 1)
        return 1;
    if (my_fun() != 100)
        return 1;
    if (call_static_my_fun_2() != 2)
        return 1;
    return 0;
}

int my_fun(void) {
    return 100;
}
