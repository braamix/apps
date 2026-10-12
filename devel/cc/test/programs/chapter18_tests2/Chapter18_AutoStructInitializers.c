/* Test initialization of non-nested structs with automatic storage duration,
 * including:
 * - partial initialization
 * - implicit type conversions
 * - compound and single expressions as initializers
 * - string literals as pointer and array initializers
 * */


// library functions
int strcmp(char *s1, char *s2);

// struct type def
struct s {
    char *one_msg;
    unsigned char two_arr[3];
    struct s *three_self_ptr;
    double four_d;
    double *five_d_ptr;
};

// validation functions defined in library
int vfull(struct s *ptr);
int vpart(struct s *ptr, char *expected_msg);
int vconv(struct s *ptr);
int vtwo(struct s *ptr1, struct s *ptr2);
/* Test initialization of non-nested structs with automatic storage duration,
 * including:
 * - partial initialization
 * - implicit type conversions
 * - compound and single expressions as initializers
 * - string literals as pointer and array initializers
 * */



double get_double(void) {
    return 2e12;
}

// case 1: fully initialized struct
int test_full_initialization(void) {
    struct s full = {
        // use string literals to initialize both pointers and arrays
        "I'M A STRUCT!", "SUP",
        &full,          // initialize member with pointer to self
        get_double(),   // initialize member with result of function call
        &(full.four_d)  // initialize member with pointer to other member in
                        // self
    };

    return vfull(&full);
}

// case 2: partially initialized struct
int test_partial_initialization(void) {
    static char *msg = "ANOTHER STRING LITERAL";
    static struct s bk;  // zeroed static stands in for calloc(1, sizeof(struct s))
    struct s partial = {
        msg,         // initialize member from variable
        {'a', 'b'},  // partially initialize array
        &bk          // initialize ptr with zeroed static backing storage
        // don't initialize last element
    };

    return vpart(&partial, msg);
}

// case 3: implicit type conversions for struct members
int test_implicit_type_conversions(void) {
    static int i = 3000;
    static char mbuf[5];  // non-null static buffer stands in for malloc(5)
    static double zd;     // zeroed static stands in for calloc(1, sizeof(double))

    struct s converted = {
        mbuf,                   // non-null char *
        {i / 2, i / 3, i * 4},  // truncate ints to chars: 220, 232, and 224
        0l,                     // convert null pointer constant to null pointer
        i - 1,                  // convert int to double
        &zd                     // zeroed double *
    };

    return vconv(&converted);
}

// case 4: initialize with single expression instead of compound initiailizer
int test_single_exp_initializer(void) {
    double d = 123.4;
    struct s s1 = {"YET ANOTHER STRING", "XY", &s1, 150.0, &d};
    struct s s2 = s1;

    return vtwo(&s1, &s2);
}

int main(void) {
    if (!test_full_initialization()) {
        return 1;
    }

    if (!test_partial_initialization()) {
        return 2;
    }

    if (!test_implicit_type_conversions()) {
        return 3;
    }

    if (!test_single_exp_initializer()) {
        return 4;
    }

    return 0;  // success
}
/* Test initialization of non-nested structs with automatic storage duration,
 * including:
 * - partial initialization
 * - implicit type conversions
 * - compound and single expressions as initializers
 * - string literals as pointer and array initializers
 * */


int vfull(struct s *ptr) {
    if (strcmp(ptr->one_msg, "I'M A STRUCT!") || ptr->two_arr[0] != 'S' ||
        ptr->two_arr[1] != 'U' || ptr->two_arr[2] != 'P' ||
        ptr->three_self_ptr != ptr || ptr->four_d != 2e12 ||
        *ptr->five_d_ptr != 2e12) {
        return 0;
    }

    return 1;  // success
}

int vpart(struct s *ptr, char *expected_msg) {
    if (ptr->one_msg != expected_msg || ptr->two_arr[0] != 'a' ||
        ptr->two_arr[1] != 'b') {
        return 0;
    }

    // validate ptr->three_self_ptr by making sure one element in it is 0
    if (ptr->three_self_ptr->one_msg) {
        return 0;
    }

    // validate elements that weren't explicitly initialized are 0
    if (ptr->two_arr[2] || ptr->four_d || ptr->five_d_ptr) {
        return 0;
    }

    return 1;  // success
}
int vconv(struct s *ptr) {
    if (!ptr->one_msg ||  // just validate that this pointer isn't null
        ptr->two_arr[0] != 220 || ptr->two_arr[1] != 232 ||
        ptr->two_arr[2] != 224 || ptr->three_self_ptr ||
        ptr->four_d != 2999.0 || *ptr->five_d_ptr != 0.0) {
        return 0;
    }

    return 1;  // success
}

int vtwo(struct s *ptr1, struct s *ptr2) {
    // validate elements of ptr2
    if (strcmp(ptr2->one_msg, "YET ANOTHER STRING") ||
        ptr2->one_msg != ptr1->one_msg ||  // both one_msg members point to same
                                           // string literal
        // contents of two_arr copied from s1 to s2
        ptr2->two_arr[0] != 'X' || ptr2->two_arr[1] != 'Y' ||
        ptr2->three_self_ptr !=
            ptr1 ||  // ptr2->three_self_ptr is ptr1, not to itself
        ptr2->four_d != 150.0 ||
        *ptr1->five_d_ptr != 123.4) {
        return 0;
    }

    // ptr1->two_arr and ptr2->two_arr are distinct arrays with different
    // addresses, even though contents are the same
    if (ptr1->two_arr == ptr2->two_arr) {
        return 0;
    }
    return 1;  // success
}

