/* Test that we return a wide range of struct types according to the ABI */

int strcmp(char *s1, char *s2);
int strncmp(char *s1, char *s2, unsigned long n);

struct one_int {
    int i;
    char c;
};

struct one_int_exactly {
    unsigned long l;
};

struct two_ints {
    char c;
    int arr[3];
};

struct two_ints_nested {
    struct one_int a;
    struct one_int b;
};

struct twelve_bytes {
    int i;
    char arr[8];
};

struct one_xmm {
    double d;
};

struct two_xmm {
    double d[2];
};

struct int_and_xmm {
    char c;
    double d;
};

struct xmm_and_int {
    struct one_xmm dbl;
    char c[3];
};

struct odd_size {
    char arr[5];
};

struct memory {
    double d;
    char c[3];
    long l;
    int i;
};

// returning structures

struct one_int return_int_struct(void);
struct twelve_bytes r_2int(void);
struct one_xmm return_double_struct(void);
struct two_xmm r_2dbl(void);
struct xmm_and_int r_mix(void);
struct int_and_xmm r_mix2(void);
struct memory return_on_stack(void);

// return on stack + pass other int params
struct memory pass_and_return_regs(int i, double d, struct int_and_xmm strct,
                                   int c, struct two_ints t_i, long l,
                                   struct one_int_exactly o_i_e, int c2);
/* Test that we return a wide range of struct types according to the ABI */

int main(void) {
    struct one_int_exactly one_long = {567890l};
    struct two_ints two_ints = {'_', {5, 6, 7}};
    struct int_and_xmm int_and_xmm = {'p', 4.56};

    // returning structures

    struct one_int s1 = return_int_struct();
    if (s1.i != 1 || s1.c != 2) {
        return 1;
    }

    struct twelve_bytes s2 = r_2int();
    if (s2.i != 10 || strncmp(s2.arr, "12345678", sizeof s2.arr))
        return 2;

    struct one_xmm s3 = return_double_struct();
    if (s3.d != 100.625)
        return 3;
    struct two_xmm s4 = r_2dbl();
    if (s4.d[0] != 8.8 || s4.d[1] != 7.8)
        return 4;

    struct xmm_and_int s5 = r_mix();
    if (s5.dbl.d != 10.0 || strcmp(s5.c, "AB"))
        return 5;

    struct int_and_xmm s6 = r_mix2();
    if (s6.c != 127 || s6.d != 34e16)
        return 6;

    struct memory s7 = return_on_stack();
    if (s7.d != 1.25 || strcmp(s7.c, "XY") || s7.l != 100l || s7.i != 44)
        return 7;

    s7 = pass_and_return_regs(6, 4.0, int_and_xmm, 5, two_ints, 77, one_long,
                              99);
    // something was clobbered or set incorrectly in retval
    if (s7.d || s7.c[0] || s7.c[1] || s7.c[2])
        return 8;

    // i was set to indicate problem w/ parameter passing
    if (s7.i)
        return 9;

    if (s7.l != 100)
        return 10;  // l field was clobbered or set incorrectly

    // success!
    return 0;
}
/* Test that we return a wide range of struct types according to the ABI */

struct one_int return_int_struct(void) {
    struct one_int retval = {1, 2};
    return retval;
}

struct twelve_bytes r_2int(void) {
    struct twelve_bytes retval = {10, "12345678"};
    return retval;
}

struct one_xmm return_double_struct(void) {
    struct one_xmm retval = {100.625};
    return retval;
}
struct two_xmm r_2dbl(void) {
    struct two_xmm retval = {{8.8, 7.8}};
    return retval;
}
struct xmm_and_int r_mix(void) {
    struct xmm_and_int retval = {{10.0}, "AB"};
    return retval;
}
struct int_and_xmm r_mix2(void) {
    struct int_and_xmm retval = {127, 34e16};
    return retval;
}
struct memory return_on_stack(void) {
    struct memory retval = {1.25, "XY", 100l, 44};
    return retval;
}

int leaf_call(struct two_ints t_i, int c, double d) {
    // validate t_i
    if (t_i.c != '_' || t_i.arr[0] != 5 || t_i.arr[1] != 6 || t_i.arr[2] != 7) {
        return 0;
    }

    // validate c1 and d1 (originally passed in a struct int_and_xmm)
    if (c != 'p' || d != 4.56) {
        return 0;
    }
    return 1;  // success
}

struct memory pass_and_return_regs(int i, double d, struct int_and_xmm strct,
                                   int c, struct two_ints t_i, long l,
                                   struct one_int_exactly o_i_e, int c2) {
    // include a stack variable to make sure it doen't overwrite return value
    // pointer or vice versa
    char stackbytes[8] = "ZYXWVUT";
    struct memory retval = {0, {0, 0, 0}, 0, 0};

    // make another function call to ensure that passing parameters
    // doesn't overwrite return address in RDI or other struct eightybtes
    // passed in registers; validate t_i and strct while we're at it
    if (!leaf_call(t_i, strct.c, strct.d)) {
        retval.i = 1;
        return retval;
    }
    // validate scalar params
    if (i != 6 || d != 4.0 || c != 5 || l != 77 || c2 != 99) {
        retval.i = 2;
        return retval;
    }
    // validate remainign struct
    if (o_i_e.l != 567890) {
        retval.i = 3;
        return retval;
    }

    // validate stackbytes
    if (strcmp(stackbytes, "ZYXWVUT")) {
        retval.i = 4;
        return retval;
    }
    retval.l = 100;
    return retval;  // success
}

