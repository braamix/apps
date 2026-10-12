/* Test that we treat union tags, function/variable names, and each
 * struct/union's member names as separate namespaces.
 */

// Different unions/structs can use same member names
union u1 {
    int a;
};
union u2 {
    double a;
    long l;
};
struct s {
    char a[2];
};

int sharemem(void) {
    union u1 var1 = {10};
    union u2 var2 = {2.5};
    struct s var3 = {{1, 2}};
    if (var1.a != 10 || var2.a != 2.5 || var3.a[0] != 1) {
        return 0;
    }

    return 1;  // success
}

// you can use the same identiifer as a struct tag, member name, and variable
// name
union u {
    int u;
};

int samevar(void) {
    union u u = {100};
    if (u.u != 100) {
        return 0;
    }

    return 1;  // success
}

// you can use the same identifier as a union tag and function name
int f(void) {
    return 10;
}

union f {
    int f;
};

int samefun(void) {
    union f x;
    x.f = f();
    if (x.f != 10) {
        return 0;  // fail
    }

    return 1;  // success
}

int main(void) {
    if (!sharemem()) {
        return 1;  // fail
    }

    if (!samevar()) {
        return 2;  // fail
    }

    if (!samefun()) {
        return 3;  // fail
    }

    return 0;  // success
}

