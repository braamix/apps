int strcmp(char *s1, char *s2);

struct small {
    int a;
    long b;
};

struct s {
    char arr[3];
    struct small inner;
};

struct with_end_padding {
    int a;
    int b;
    char c;
};
/* Test copying whole structs via Load and Store,
 * including copies of the form *x = y , x = *y,
 * and reads and writes of array elements,
 * with a mix of static and automatic structs
 * */


// case 1: *x = y
int tc_tptr(void) {
    struct s y = {"!?", {-20, -30}};
    static struct s backing1;
    struct s *x = &backing1;
    *x = y;

    // validate
    if (strcmp(x->arr, "!?") || x->inner.a != -20 || x->inner.b != -30) {
        return 0;
    }

    return 1;  // success
}

// case 2: x = *y
int tc_fptr(void) {
    static struct s my_struct = {"()", {77, 78}};
    struct s *y = &my_struct;
    struct s x = {"", {0, 0}};
    x = *y;

    // validate
    if (strcmp(x.arr, "()") || x.inner.a != 77 || x.inner.b != 78) {
        return 0;
    }

    return 1;  // success
}

// case 3: *x = *y
int tc_tfptr(void) {
    struct s my_struct = {"+-", {1000, 1001}};
    struct s *y = &my_struct;
    static struct s backing3;
    struct s *x = &backing3;
    *x = *y;

    // validate
    if (strcmp(x->arr, "+-") || x->inner.a != 1000 || x->inner.b != 1001) {
        return 0;
    }

    return 1;  // success
}

// case 4: arr[i] = y
int tc_telem(void) {
    struct s y = {"\n\t", {10000, 20000}};
    static struct s arr[3];

    arr[1] = y;

    // validate
    if (strcmp(arr[1].arr, "\n\t") || arr[1].inner.a != 10000 ||
        arr[1].inner.b != 20000) {
        return 0;
    }

    // make sure adjoining array elements are unchanged
    if (arr[0].inner.a || arr[0].inner.b || arr[2].arr[0] || arr[2].arr[1]) {
        return 0;
    }
    return 1;  // success
}

// case 5: x = arr[i]
int tc_felem(void) {
    struct s arr[3] = {
        {"AB", {-3000, -4000}}, {"CD", {-5000, -6000}}, {"EF", {-7000, -8000}}};

    struct s x = {"", {0, 0}};
    x = arr[1];
    // validate
    if (strcmp(x.arr, "CD") || x.inner.a != -5000 || x.inner.b != -6000) {
        return 0;
    }

    return 1;  // success
}

// case 6: arr[i] = arr[j]
int tc_tfelem(void) {
    struct s arr[3] = {
        {"AB", {-3000, -4000}}, {"CD", {-5000, -6000}}, {"EF", {-7000, -8000}}};

    arr[0] = arr[2];
    // validate all elements

    // element 0
    if (strcmp(arr[0].arr, "EF") || arr[0].inner.a != -7000 ||
        arr[0].inner.b != -8000) {
        return 0;
    }

    // element 1
    if (strcmp(arr[1].arr, "CD") || arr[1].inner.a != -5000 ||
        arr[1].inner.b != -6000) {
        return 0;
    }

    // element 2
    if (strcmp(arr[2].arr, "EF") || arr[2].inner.a != -7000 ||
        arr[2].inner.b != -8000) {
        return 0;
    }

    return 1;  // success
}

// case 7: copy struct w/ trailing padding to array element
int tc_pad(void) {
    struct with_end_padding arr[3] = {{0, 1, 2}, {3, 4, 5}, {6, 7, 8}};
    struct with_end_padding elem = {9, 9, 9};
    arr[1] = elem;
    if (arr[0].a != 0 || arr[0].b != 1 || arr[0].c != 2 || arr[1].a != 9 ||
        arr[1].b != 9 || arr[1].c != 9 || arr[2].a != 6 || arr[2].b != 7 ||
        arr[2].c != 8) {
        return 0;
    }

    return 1;  // success
}

int main(void) {
    if (!tc_tptr()) {
        return 1;
    }

    if (!tc_fptr()) {
        return 2;
    }

    if (!tc_tfptr()) {
        return 3;
    }
    if (!tc_telem()) {
        return 4;
    }
    if (!tc_felem()) {
        return 5;
    }

    if (!tc_tfelem()) {
        return 6;
    }

    if (!tc_pad()) {
        return 7;
    }
    return 0;  // success
}

