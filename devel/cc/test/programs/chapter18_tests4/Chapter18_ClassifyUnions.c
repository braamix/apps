int strcmp(char* s1, char* s2);

// union of doubles: an array member overlays a scalar member
union two_doubles {
    double arr[2];
    double single;
};

// union whose first member is an integer, second a char
union int_char {
    int i;
    char c;
};

// nested unions
union one_dbl {
    double d1;
    double d2;
};
union nest {
    union one_dbl od;
    double x;
};

// union containing a struct (which itself contains a union)
struct has_u {
    unsigned int i;
    union int_char u;
};
union us {
    double d;
    struct has_u s;
    unsigned long ul;
};

// struct containing a union
struct dbl_struct {
    union one_dbl m1;
    double m2;
};

// union containing an array of unions
union uarr {
    union int_char a[2];
};

// union containing an array of structs
struct small {
    char arr[3];
    signed char sc;
};
union has_sa {
    struct small a[3];
};

// two-word union: an 11-char array overlays an int
union char_arr {
    char arr[11];
    int i;
};

int t_twodbl(union two_doubles u);
int t_intchr(union int_char u);
int t_nest(union nest u);
int t_ustrct(union us u);
int t_dstrct(struct dbl_struct s);
int t_uarr(union uarr u);
int t_sarr(union has_sa u);
int t_carr(union char_arr u);

int main(void) {
    // union of doubles passed by value
    union two_doubles td = { {10.0, 11.0} };
    if (!t_twodbl(td)) {
        return 1;
    }

    // int/char union, written and read through its int member
    union int_char ic = { 1000000 };
    if (!t_intchr(ic)) {
        return 2;
    }

    // nested unions
    union nest nst;
    nst.od.d1 = -2.345e6;
    if (!t_nest(nst)) {
        return 3;
    }

    // union containing a struct
    union us u;
    u.s.i = 4294954951u;
    u.s.u.c = 60;
    if (!t_ustrct(u)) {
        return 4;
    }

    // struct containing a union
    struct dbl_struct ds;
    ds.m1.d1 = -2.345e6;
    ds.m2 = 123.45;
    if (!t_dstrct(ds)) {
        return 5;
    }

    // array of unions
    union uarr ua;
    ua.a[0].i = -20;
    ua.a[1].i = -30;
    if (!t_uarr(ua)) {
        return 6;
    }

    // array of structs
    union has_sa hsa = { {
        {"AS", 10}, {"DF", 11}, {"GH", 12}
    } };
    if (!t_sarr(hsa)) {
        return 7;
    }

    // two-word union
    union char_arr ca = { "CHARS" };
    if (!t_carr(ca)) {
        return 8;
    }

    return 0; // success
}

int t_twodbl(union two_doubles u) {
    return u.arr[0] == 10.0 && u.arr[1] == 11.0 && u.single == 10.0;
}
int t_intchr(union int_char u) {
    return u.i == 1000000;
}
int t_nest(union nest u) {
    return u.od.d1 == -2.345e6 && u.od.d2 == -2.345e6;
}
int t_ustrct(union us u) {
    return u.s.i == 4294954951u && u.s.u.c == 60;
}
int t_dstrct(struct dbl_struct s) {
    return s.m1.d1 == -2.345e6 && s.m1.d2 == -2.345e6 && s.m2 == 123.45;
}
int t_uarr(union uarr u) {
    return u.a[0].i == -20 && u.a[1].i == -30;
}
int t_sarr(union has_sa u) {
    return strcmp(u.a[0].arr, "AS") == 0 && u.a[0].sc == 10
        && strcmp(u.a[1].arr, "DF") == 0 && u.a[1].sc == 11
        && strcmp(u.a[2].arr, "GH") == 0 && u.a[2].sc == 12;
}
int t_carr(union char_arr u) {
    return strcmp(u.arr, "CHARS") == 0;
}

