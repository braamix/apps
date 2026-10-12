struct inner { double d; int i; };
struct outer { struct inner s; struct inner *ptr; long l; };
int modify_simple_struct(struct inner s);
int modify_nested_struct(struct outer s);
int main(void) {
    struct inner s_inner = {2.0, 3};
    if (!modify_simple_struct(s_inner)) return 1;
    if (s_inner.d != 2.0 || s_inner.i != 3) return 2;
    struct outer s_o = {{4.0, 5}, &s_inner, 1000l};
    if (!modify_nested_struct(s_o)) return 3;
    if (s_o.s.d != 4.0 || s_o.s.i != 5 || s_o.l != 1000l) return 4;
    if (s_o.ptr != &s_inner) return 5;
    if (s_o.ptr->d != 10.0 || s_o.ptr->i != 11) return 6;
    return 0;
}
int modify_simple_struct(struct inner s) {
    struct inner copy = s;
    s.d = 0.0;
    if (s.d || s.i != 3) return 0;
    if (copy.d != 2.0 || copy.i != 3) return 0;
    return 1;
}
int modify_nested_struct(struct outer s) {
    struct outer copy = s;
    s.l = 10;
    s.s.i = 200;
    s.ptr->d = 10.0;
    s.ptr->i = 11;
    if (s.s.i != 200 || s.s.d != 4.0 || s.l != 10 || s.ptr->d != 10.0 ||
        s.ptr->i != 11) return 0;
    if (copy.s.i != 5 || copy.s.d != 4.0 || copy.l != 1000 ||
        copy.ptr->d != 10.0 || copy.ptr->i != 11) return 0;
    return 1;
}

