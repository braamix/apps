/* Test initialization of non-nested static structs, including:
 * - partial initialization
 * - implicit conversion of scalar elements
 * - array decay of string literals
 */

/* Test initialization of non-nested static structs, including:
 * - partial initialization
 * - implicit conversion of scalar elements
 * - array decay of string literals
 */

int strcmp(char *s1, char *s2);

struct s {
    double one_d;
    char *two_msg;
    unsigned char three_arr[3];
    int four_i;
};

// static structures defined in client
extern struct s uninitialized;
extern struct s partial;
extern struct s partial_with_array;
extern struct s converted;

// validation functions defined in library

// case 1: struct with no explicit initializer should be all zeros
int test_uninitialized(void);

// case 2: partially initialized struct
int test_partially_initialized(void);

// case 3: partially initialized array w/in struct
int test_partial_inner_init(void);

// case 4: implicit conversion of scalar elements
int test_implicit_conversion(void);

// structs defined in client but visible here
// validation functions defined here

// case 1: struct with no explicit initializer should be all zeros
// struct s uninitialized;
int test_uninitialized(void) {
    // make sure all elements are zero
    if (uninitialized.one_d || uninitialized.two_msg ||
        uninitialized.three_arr[0] || uninitialized.three_arr[1] ||
        uninitialized.three_arr[2] || uninitialized.four_i) {
        return 0;
    }
    return 1;  // success
}

// case 2: partially initialized struct
// struct s partial = {1.0, "Hello"};
int test_partially_initialized(void) {
    // validate first two elements
    if (partial.one_d != 1.0 || strcmp(partial.two_msg, "Hello")) {
        return 0;
    }

    // validate that remaining elements are zero
    if (partial.three_arr[0] || partial.three_arr[1] || partial.three_arr[2] ||
        partial.four_i) {
        return 0;
    }

    return 1;  // success
}

// case 3: partially initialized array w/in struct
// struct s partial with_array = {3.0, "!", {1}, 2};
int test_partial_inner_init(void) {
    // validate explicitly initialzed elements
    if (partial_with_array.one_d != 3.0 ||
        strcmp(partial_with_array.two_msg, "!") ||
        partial_with_array.three_arr[0] != 1 ||
        partial_with_array.four_i != 2) {
        return 0;
    }

    // validate that last two elements of arr are 0
    if (partial_with_array.three_arr[1] || partial_with_array.three_arr[2]) {
        return 0;
    }

    return 1;  // success
}

// case 4: implicit conversion of scalar elements
/*
    struct s converted = {
        1152921504606846977l,  // 1152921504606846976.0
        0l,                   // null ptr
        "abc",                // {'a', 'b', 'c'}
        17179869189l          // 5
    };
*/
int test_implicit_conversion(void) {
    // validate elements
    if (converted.one_d != 1152921504606846976.0 || converted.two_msg ||
        converted.three_arr[0] != 'a' || converted.three_arr[1] != 'b' ||
        converted.three_arr[2] != 'c' || converted.four_i != 5) {
        return 0;
    }

    return 1;  // success
}

/* Test initialization of non-nested static structs, including:
 * - partial initialization
 * - implicit conversion of scalar elements
 * - array decay of string literals
 */

// case 1: struct with no explicit initializer should be all zeros
struct s uninitialized;

// case 2: partially initialized struct
struct s partial = {1.0, "Hello"};

// case 3: partially initialized array w/in struct
struct s partial_with_array = {3.0, "!", {1}, 2};

// case 4: implicit conversion of scalar elements
struct s converted = {
    1152921504606846977l,  // 1152921504606846976.0
    0l,                    // null ptr
    "abc",                 // {'a', 'b', 'c'}
    17179869189l           // 5
};

int main(void) {
    if (!test_uninitialized()) {
        return 1;
    }

    if (!test_partially_initialized()) {
        return 2;
    }

    if (!test_partial_inner_init()) {
        return 3;
    }

    if (!test_implicit_conversion()) {
        return 4;
    }

    return 0;  // success
}

