/* Test that we classify structure parameters correctly,
 * by passing a variety of structures as arguments.
 * Each test function takes only one argument.
 * */


int strcmp(char *s1, char *s2);

// from Listing 18-39
struct twelve_bytes {
    int i;
    char arr[8];
};  // two INTEGER eightbytes

// from Listing 18-40
struct inner {
    int i;
    char ch2;
};

struct nested_ints {
    char ch1;
    struct inner nested;
};  // two INTEGER eightbytes

// from Listing 18-41
struct flattened_ints {
    char c;
    int i;
    char a;
};  // two INTEGER eightbytes

// From uncaptioned listing in "Classifying Eightbytes" section
struct large {
    int i;
    double d;
    char arr[10];
};  // four MEMORY eightbytes

// Three structure declarations from Listing 18-42
struct two_ints {
    int i;
    int i2;
};  // one INTEGER eightbyte

struct nested_double {
    double array[1];
};  // one SSE eightbyte

struct two_eightbytes {
    double d;
    char c;
};  // one SSE eightbyte, one INTEGER eightbyte

// From Listing 18-47
struct pass_in_memory {
    double w;
    double x;
    int y;
    long z;
};  // four MEMORY eightbytes

// validation functions defined in library
int test_twelve_bytes(struct twelve_bytes s);
int t_nints(struct nested_ints s);
int test_flattened_ints(struct flattened_ints s);
int test_large(struct large s);
int t_2ints(struct two_ints s);
int t_ndbl(struct nested_double s);
int t_2eb(struct two_eightbytes s);
int test_pass_in_memory(struct pass_in_memory s);
/* Test that we classify structure parameters correctly,
 * by passing a variety of structures as arguments.
 * Each test function takes only one argument.
 * */


int main(void) {
    struct twelve_bytes s1 = {0, "LMNOPQR"};
    if (!test_twelve_bytes(s1)) {
        return 1;
    }

    struct nested_ints s2 = {127, {2147483647, 255}};
    if (!t_nints(s2)) {
        return 2;
    }

    struct flattened_ints s3 = {127, 2147483647, 255};
    if (!test_flattened_ints(s3)) {
        return 3;
    }

    struct large s4 = {200000, 23.25, "ABCDEFGHI"};
    if (!test_large(s4)) {
        return 4;
    }

    struct two_ints s5 = {999, 888};
    if (!t_2ints(s5)) {
        return 5;
    }

    struct nested_double s6 = {{25.125e3}};
    if (!t_ndbl(s6)) {
        return 6;
    }

    struct two_eightbytes s7 = {1000., 'x'};
    if (!t_2eb(s7)) {
        return 7;
    }

    struct pass_in_memory s8 = {1.0e18, -1.0e18, -2147483647, -1099511627775l};
    if (!test_pass_in_memory(s8)) {
        return 8;
    }

    return 0; // success
}
/* Test that we classify structure parameters correctly,
 * by passing a variety of structures as arguments.
 * Each test function takes only one argument.
 * */


int test_twelve_bytes(struct twelve_bytes s) {
    if (s.i != 0 || strcmp(s.arr, "LMNOPQR")) {
        return 0;
    }
    return 1;  // success
}
int t_nints(struct nested_ints s) {
    if (s.ch1 != 127 || s.nested.i != 2147483647 || s.nested.ch2 != 255) {
        return 0;
    }
    return 1;  // success
}
int test_flattened_ints(struct flattened_ints s) {
    if (s.c != 127 || s.i != 2147483647 || s.a != 255) {
        return 0;
    }

    return 1;  // success
}
int test_large(struct large s) {
    if (s.i != 200000 || s.d != 23.25 || strcmp(s.arr, "ABCDEFGHI")) {
        return 0;
    }

    return 1;  // success
}
int t_2ints(struct two_ints s) {
    if (s.i != 999 || s.i2 != 888) {
        return 0;
    }

    return 1;  // success
}
int t_ndbl(struct nested_double s) {
    if (s.array[0] != 25.125e3) {
        return 0;
    }

    return 1;  // success
}
int t_2eb(struct two_eightbytes s) {
    if (s.d != 1000. || s.c != 'x') {
        return 0;
    }

    return 1;  // success
}
int test_pass_in_memory(struct pass_in_memory s) {
    if (s.w != 1.0e18 || s.x != -1.0e18 || s.y != -2147483647 ||
        s.z != -1099511627775l) {
        return 0;
    }

    return 1;  // success
}

