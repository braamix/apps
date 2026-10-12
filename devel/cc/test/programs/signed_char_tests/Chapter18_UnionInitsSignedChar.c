// library functions
int strcmp(char *s1, char *s2);

union simple {
    double d;
    char c;
    int *ptr;
};

union inner {
    char arr[9];
};

struct my_struct {
    long l;
    union inner u;
    int i;
};

union nested {
    struct my_struct str;
    union simple s;
    long l;
};

int vsimp(union simple *ptr);
int vsimpcv(union simple *ptr);
int vnest(union nested *ptr);
int vnestp(union nested *ptr);

// Test initialization of unions with automatic storage duration

int tsimp(void) {
    // initialize simple union w/ only scalar members
    union simple x = { 123.45 };
    return vsimp(&x);
}

int tsimpcv(void) {
    // initialize simple union where the unsigned value is implicitly converted to the
    // double member (2^32 is exactly representable in the BESM-6 native FP format)
    union simple x = { 4294967296UL };
    return vsimpcv(&x);
}

int tnest(void) {
    // initalize nested union where first member is a structure
    union nested x = { {4294967395l, {{-1, -2, -3, -4, -5, -6, -7, -8, -9}}} };
    return vnest(&x);
}

int tnestp(void) {
    // initialize union where inner subobject is a partly initialized struct
    union nested x = { {900037203685l, {"STRING"}} };
    return vnestp(&x);
}

int main(void) {
    if (!tsimp()) {
        return 1;
    }

    if (!tsimpcv()) {
        return 2;
    }

    if (!tnest()) {
        return 3;
    }

    if (!tnestp()) {
        return 4;
    }

    return 0;
}

// Test initialization of unions with both automatic and static storage duration

int vsimp(union simple* ptr) {
    return (ptr->d == 123.45);
}

int vsimpcv(union simple* ptr) {
    return (ptr->d == 4294967296.);
}

int vnest(union nested* ptr) {
    if (ptr->str.l != 4294967395l) {
        return 0; // fail
    }

    for (int i = 0; i < 9; i = i + 1) {
        if (ptr->str.u.arr[i] != -1 - i) {
            return 0;  // fail
        }
    }

    return 1; // success
}
    
int vnestp(union nested* ptr) {
    if (ptr->str.l != 900037203685l) {
        return 0; // fail
    }

    if (strcmp(ptr->str.u.arr, "STRING")) {
        return 0; // fail
    }

    return 1; // success
}

