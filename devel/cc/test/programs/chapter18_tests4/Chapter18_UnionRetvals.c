int strcmp(char* s1, char* s2);
void exit(int status);

// one-word union with only double members
union one_dbl {
    double d1;
    double d2;
};

// one-word union that can hold either a double or a char
union one_int {
    double d;
    char c;
};

// nested one-word unions
union one_int_nested {
    union one_int oi;
    union one_dbl od;
};

// two-word: struct containing a union, plus a trailing double
struct dbl_struct {
    union one_dbl m1;
    double m2;
};
union has_dbl_struct {
    struct dbl_struct m1;
};

// two-word: two parallel arrays overlaid
union two_arrs {
    double dbl_arr[2];
    long long_arr[2];
};

// two-word: a pointer overlaid with a struct (char + double)
struct char_first {
    char c;
    double d;
};
union scalar_and_struct {
    long *ptr;
    struct char_first cfe;
};

// two-word: a double overlaid with a struct (double + int)
struct int_second {
    double d;
    int i;
};
union xmm_and_gp {
    double d;
    struct int_second ise;
};

// four-word ("in memory"): array of two-word unions
union gp_and_xmm {
    double d_arr[2];
    char c;
};
union contains_union_array {
    union gp_and_xmm arr[2];
};

// multi-word char union, returned in memory
union lotsa_chars {
    char more_chars[18];
    char fewer_chars[5];
};

// struct containing a union, returned by value
union uneven {
    char arr[5];
    unsigned char uc;
};
struct has_uneven_union {
    int i;
    union uneven u;
};

union one_dbl r_onedbl(void);
union one_int_nested r_oin(void);
union has_dbl_struct r_hds(void);
union two_arrs r_arrs(void);
union scalar_and_struct r_scst(void);
union xmm_and_gp r_xgp(void);
union contains_union_array r_cua(void);
union lotsa_chars r_pmem(int i1, union scalar_and_struct int_and_dbl,
    union two_arrs ta, int i2, union contains_union_array big_union,
    union one_int_nested oin);
struct has_uneven_union r_swu(void);

int main(void) {
    // return a one-word union (in the accumulator)
    union one_dbl od = r_onedbl();
    if (!(od.d1 == 245.5 && od.d2 == 245.5)) {
        return 1;
    }

    // return a nested one-word union
    union one_int_nested oin = r_oin();
    if (oin.oi.d != -9876.5) {
        return 2;
    }

    // return a two-word union (sret)
    union has_dbl_struct two_xmm = r_hds();
    if (!(two_xmm.m1.m1.d1 == 1234.5 && two_xmm.m1.m2 == 6789.)) {
        return 3;
    }

    // return a two-word union, write/read distinct array slots
    union two_arrs ta = r_arrs();
    if (ta.dbl_arr[0] != 66.75 || ta.long_arr[1] != -4294967300l) {
        return 4;
    }

    // return a union that overlays a struct (char + double)
    union scalar_and_struct int_and_dbl = r_scst();
    if (int_and_dbl.cfe.c != 115 || int_and_dbl.cfe.d != 222222.25) {
        return 5;
    }

    // return a union that overlays a struct (double + int)
    union xmm_and_gp dbl_and_int = r_xgp();
    if (dbl_and_int.ise.d != -50000.125 || dbl_and_int.ise.i != -3000) {
        return 6;
    }

    // return a four-word union ("in memory")
    union contains_union_array big_union = r_cua();
    if (!(big_union.arr[0].d_arr[0] == -2000e-4 && big_union.arr[0].d_arr[1] == -3000e-4
        && big_union.arr[1].d_arr[0] == 20000e10 && big_union.arr[1].d_arr[1] == 5000e11)) {
        return 7;
    }

    // pass aggregates by value AND return one in memory; make sure returning in
    // memory doesn't disturb param passing
    union lotsa_chars chars_union = r_pmem(1, int_and_dbl, ta, 25, big_union, oin);
    if (strcmp(chars_union.more_chars, "ABCDEFGHIJKLMNOPQ") != 0) {
        return 8;
    }

    // return a struct that contains a union
    struct has_uneven_union s = r_swu();
    if (s.i != -8765 || strcmp(s.u.arr, "DONE") != 0) {
        return 9;
    }

    return 0; // success
}

union one_dbl r_onedbl(void) {
    union one_dbl result = { 245.5 };
    return result;
}

union one_int_nested r_oin(void) {
    union one_int_nested result = { {-9876.5} };
    return result;
}

union has_dbl_struct r_hds(void) {
    union has_dbl_struct result = { { {1234.5}, 6789. } };
    return result;
}

union two_arrs r_arrs(void) {
    union two_arrs result;
    result.dbl_arr[0] = 66.75;
    result.long_arr[1] = -4294967300l;
    return result;
}

union scalar_and_struct r_scst(void) {
    union scalar_and_struct result;
    result.cfe.c = 115;
    result.cfe.d = 222222.25;
    return result;
}

union xmm_and_gp r_xgp(void) {
    union xmm_and_gp result;
    result.ise.d = -50000.125;
    result.ise.i = -3000;
    return result;
}

union contains_union_array r_cua(void) {
    union contains_union_array result = {
        {
            {{-2000e-4, -3000e-4}}, {{20000e10, 5000e11}}
        }
    };
    return result;
}

union lotsa_chars r_pmem(int i1, union scalar_and_struct int_and_dbl,
    union two_arrs ta, int i2, union contains_union_array big_union,
    union one_int_nested oin) {

    // validate scalar params
    if (i1 != 1 || i2 != 25) {
        exit(-1);
    }

    // validate non-scalar params
    if (int_and_dbl.cfe.c != 115 || int_and_dbl.cfe.d != 222222.25) {
        exit(-2);
    }
    if (ta.dbl_arr[0] != 66.75 || ta.long_arr[1] != -4294967300l) {
        exit(-3);
    }
    if (!(big_union.arr[0].d_arr[0] == -2000e-4 && big_union.arr[0].d_arr[1] == -3000e-4
        && big_union.arr[1].d_arr[0] == 20000e10 && big_union.arr[1].d_arr[1] == 5000e11)) {
        exit(-4);
    }
    if (oin.oi.d != -9876.5) {
        exit(-5);
    }

    // construct the result
    union lotsa_chars result = { "ABCDEFGHIJKLMNOPQ" };
    return result;
}

struct has_uneven_union r_swu(void) {
    struct has_uneven_union result = {
        -8765, {"DONE"}
    };
    return result;
}

