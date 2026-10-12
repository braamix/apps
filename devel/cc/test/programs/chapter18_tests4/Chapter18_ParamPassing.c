int strcmp(char* s1, char* s2);

// two-word union: an array member overlays a scalar member
union two_doubles {
    double arr[2];
    double single;
};

// one-word union that can hold either a double or a char
union one_int {
    double d;
    char c;
};

// one-word union of int and char
union char_int {
    char c;
    int i;
};

// struct containing a union
struct has_union {
    unsigned int i;
    union char_int u;
};

// two-word union: an 11-char array overlays an int
union char_arr {
    char arr[11];
    int i;
};

int p_mix(int i1, int i2, struct has_union s, double d1, union two_doubles td,
    union one_int oi, int i3, int i4, int i5);
int p_gpmem(union two_doubles td, struct has_union s, int i1, int i2, int i3,
    int i4, int i5, int i6, union one_int oi);
int p_xmmem(double d1, double d2, union two_doubles td, double d3, double d4,
    union two_doubles td2);
int p_strct(int i1, int i2, int i3, int i4, int i5, union char_arr ca);

int main(void) {
    // a mix of unions, a struct-with-union, and scalars, all passed by value
    union two_doubles td = { {10.0, 11.0} };
    union one_int oi = { 13.0 };
    struct has_union s = { 12345u, {0} };
    s.u.i = 123456789;

    if (!p_mix(1, 2, s, 4.0, td, oi, 100, 120, 130)) {
        return 1;
    }

    // aggregates placed after several scalar args (BESM-6 marshals them the same
    // by-value way x86 would spill to memory)
    if (!p_gpmem(td, s, 1, 2, 3, 4, 5, 6, oi)) {
        return 2;
    }

    // two two-word double unions among double scalars
    union two_doubles td2 = { {33e4, 55e6} };
    if (!p_xmmem(1.0, 2.0, td, 3.0, 4.0, td2)) {
        return 3;
    }

    // a two-word char-array union passed after scalar args
    union char_arr ca = { "CHARS" };
    if (!p_strct(1, 2, 3, 4, 5, ca)) {
        return 4;
    }

    return 0; // success
}

int p_mix(int i1, int i2, struct has_union s, double d1, union two_doubles td,
    union one_int oi, int i3, int i4, int i5) {
    if (!(i1 == 1 && i2 == 2 && d1 == 4.0 && i3 == 100 && i4 == 120 && i5 == 130)) {
        return 0; // fail
    }
    if (!(s.i == 12345u && s.u.i == 123456789)) {
        return 0; // fail
    }
    if (!(td.arr[0] == 10.0 && td.arr[1] == 11.0)) {
        return 0; // fail
    }
    if (!(oi.d == 13.0)) {
        return 0; // fail
    }
    return 1; // success
}

int p_gpmem(union two_doubles td, struct has_union s, int i1, int i2, int i3,
    int i4, int i5, int i6, union one_int oi) {
    if (!(i1 == 1 && i2 == 2 && i3 == 3 && i4 == 4 && i5 == 5 && i6 == 6)) {
        return 0; // fail
    }
    if (!(td.arr[0] == 10.0 && td.arr[1] == 11.0)) {
        return 0; // fail
    }
    if (!(s.i == 12345u && s.u.i == 123456789)) {
        return 0; // fail
    }
    if (!(oi.d == 13.0)) {
        return 0; // fail
    }
    return 1; // success
}

int p_xmmem(double d1, double d2, union two_doubles td, double d3, double d4,
    union two_doubles td2) {
    if (!(d1 == 1.0 && d2 == 2.0 && d3 == 3.0 && d4 == 4.0)) {
        return 0; // fail
    }
    if (!(td.arr[0] == 10.0 && td.arr[1] == 11.0)) {
        return 0; // fail
    }
    if (!(td2.arr[0] == 33e4 && td2.arr[1] == 55e6)) {
        return 0; // fail
    }
    return 1; // success
}

int p_strct(int i1, int i2, int i3, int i4, int i5, union char_arr ca) {
    if (!(i1 == 1 && i2 == 2 && i3 == 3 && i4 == 4 && i5 == 5)) {
        return 0; // fail
    }
    if (strcmp(ca.arr, "CHARS") != 0) {
        return 0; // fail
    }
    return 1; // success
}

