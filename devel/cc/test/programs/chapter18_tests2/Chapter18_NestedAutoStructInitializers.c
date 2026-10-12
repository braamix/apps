/* Test initialization of nested structs with automatic storage duration,
 * including:
 * - partial initialization
 * - using mix of compound and single initializers to initialize nested structs
 * - arrays of structs, structs containing arrays
 * */


int strcmp(char *s1, char *s2);

// struct type defs
struct pair {
    int a;
    int b;
};

struct inner {
    int one_i;
    unsigned char two_arr[3];
    unsigned three_u;
};

struct outer {
    long one_l;
    struct inner two_struct;
    char *three_msg;
    double four_d;
    struct pair five_pair;
};

// validation functions defined in library
int check_full(struct outer *ptr);
int check_partial(struct outer *ptr);
int check_mixed(struct outer *ptr);
int check_array(struct outer *struct_array);
/* Test initialization of nested structs with automatic storage duration,
 * including:
 * - partial initialization
 * - using mix of compound and single initializers to initialize nested structs
 * - arrays of structs, structs containing arrays
 * */



// case 1: fully initialized struct (include some implicit conversions while
// we're at it)
int test_full_initialization(void) {
    struct outer full = {-200,
                         {-171l, {-56, -54, -53}, 40.5},
                         "Important message!",
                         -22,
                         {1, 2}};

    return check_full(&full);
}

// case 2: partially initialized struct
int test_partial_initialization(void) {
    struct outer partial = {1000,
                            {
                                1,
                                // leave two_arr and three_u ininitialized
                            },
                            "Partial"};  // leave four_d uninitialized

    return check_partial(&partial);
}

// case 3: initialize a nested struct with a single expression of struct type
// rather than a compound initializer
int test_mixed_initialization(void) {
    struct inner inner1 = {10};
    struct inner inner2 = {20, {21}, 22u};
    static int flag = 0;

    struct outer mixed = {
        200,
        flag ? inner1 : inner2,  // initialize to inner2
        "mixed",
        10.0,
        {99,
         100}  // still use compound init for second nexted struct, five_pair
    };

    return check_mixed(&mixed);
}

// case 4: initialize an array of structures
int test_array_of_structs(void) {
    struct outer s0 = {1, {2, {3, 4, 5}, 6}, "7", 8.0, {9, 10}};

    struct inner in1 = {102, {103, 104, 105}, 106};
    struct pair pair1 = {109, 110};
    struct pair pair2 = {209};

    struct outer s3 = {301};

    struct outer struct_array[4] = {
        // struct_array[0]: initialize whole array element w/ one struct
        s0,
        {101, in1, "107", 108.0, pair1},
        // struct_array[2]: partial initialization; compound initialize for one
        // subelement, single for other
        {201,
         // struct_array[2].two_struct
         {202, {203}},
         "207",
         208.0,
         pair2},
        // struct_array[3]: initialize whole array element from one partially
        // initialized struct
        s3};

    return check_array(struct_array);
}

int main(void) {
    if (!test_full_initialization()) {
        return 1;
    }

    if (!test_partial_initialization()) {
        return 2;
    }

    if (!test_mixed_initialization()) {
        return 3;
    }

    if (!test_array_of_structs()) {
        return 4;
    }

    return 0;  // success
}
/* Test initialization of nested structs with automatic storage duration,
 * including:
 * - partial initialization
 * - using mix of compound and single initializers to initialize nested structs
 * - arrays of structs, structs containing arrays
 * */


int check_full(struct outer *ptr) {
    if (ptr->one_l != -200l || ptr->two_struct.one_i != -171 ||
        ptr->two_struct.two_arr[0] != 200 ||
        ptr->two_struct.two_arr[1] != 202 ||
        ptr->two_struct.two_arr[2] != 203 || ptr->two_struct.three_u != 40u ||
        strcmp(ptr->three_msg, "Important message!") || ptr->four_d != -22. ||
        ptr->five_pair.a != 1 || ptr->five_pair.b != 2) {
        return 0;
    }

    return 1;  // success
}

int check_partial(struct outer *ptr) {
    // validate explicitly initialized members
    if (ptr->one_l != 1000 || ptr->two_struct.one_i != 1 ||
        strcmp(ptr->three_msg, "Partial")) {
        return 0;
    }

    // validate that uninitialized members are 0
    if (ptr->two_struct.two_arr[0] || ptr->two_struct.two_arr[1] ||
        ptr->two_struct.two_arr[2] || ptr->two_struct.three_u || ptr->four_d ||
        ptr->five_pair.a || ptr->five_pair.b) {
        return 0;
    }

    return 1;  // success
}

int check_mixed(struct outer *ptr) {
    // validate explicitly initialized elements
    if (ptr->one_l != 200 || ptr->two_struct.one_i != 20 ||
        ptr->two_struct.two_arr[0] != 21 || ptr->two_struct.three_u != 22u ||
        strcmp(ptr->three_msg, "mixed") || ptr->four_d != 10.0 ||
        ptr->five_pair.a != 99 || ptr->five_pair.b != 100) {
        return 0;
    }

    // validate elements that were not explicitly initialized in inner2
    if (ptr->two_struct.two_arr[1] || ptr->two_struct.two_arr[2]) {
        return 0;
    }

    return 1;  // success
}

int check_array(struct outer *struct_array) {
    // validate element 0
    if (struct_array[0].one_l != 1 || struct_array[0].two_struct.one_i != 2 ||
        struct_array[0].two_struct.two_arr[0] != 3 ||
        struct_array[0].two_struct.two_arr[1] != 4 ||
        struct_array[0].two_struct.two_arr[2] != 5 ||
        struct_array[0].two_struct.three_u != 6 ||
        strcmp(struct_array[0].three_msg, "7") ||
        struct_array[0].four_d != 8.0 || struct_array[0].five_pair.a != 9 ||
        struct_array[0].five_pair.b != 10) {
        return 0;
    }

    // validate element 1
    if (struct_array[1].one_l != 101 ||
        struct_array[1].two_struct.one_i != 102 ||
        struct_array[1].two_struct.two_arr[0] != 103 ||
        struct_array[1].two_struct.two_arr[1] != 104 ||
        struct_array[1].two_struct.two_arr[2] != 105 ||
        struct_array[1].two_struct.three_u != 106 ||
        strcmp(struct_array[1].three_msg, "107") ||
        struct_array[1].four_d != 108.0 || struct_array[1].five_pair.a != 109 ||
        struct_array[1].five_pair.b != 110) {
        return 0;
    }

    // validate element 2
    if (struct_array[2].one_l != 201 ||
        struct_array[2].two_struct.one_i != 202 ||
        struct_array[2].two_struct.two_arr[0] != 203 ||
        // remaining elements of two_struct should be 0 since they weren't
        // explicitly initialized
        struct_array[2].two_struct.two_arr[1] ||
        struct_array[2].two_struct.two_arr[2] ||
        struct_array[2].two_struct.three_u ||
        strcmp(struct_array[2].three_msg, "207") ||
        struct_array[2].four_d != 208.0 || struct_array[2].five_pair.a != 209 ||
        // five_pair.b should be 0 since it wasn't explicitly initialized
        struct_array[2].five_pair.b) {
        return 0;
    }

    // validate element 3: one_l is 301, everything else is 0
    if (struct_array[3].one_l != 301 || struct_array[3].two_struct.one_i ||
        struct_array[3].two_struct.two_arr[0] ||
        struct_array[3].two_struct.two_arr[1] ||
        struct_array[3].two_struct.two_arr[2] ||
        struct_array[3].two_struct.three_u || struct_array[3].three_msg ||
        struct_array[3].four_d || struct_array[3].five_pair.a ||
        struct_array[3].five_pair.b) {
        return 0;
    }

    return 1;  // success
}

