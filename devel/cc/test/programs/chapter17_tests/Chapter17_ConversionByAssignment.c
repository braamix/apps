/* In conversion as if by assignment, we can implicitly convert between void *
 * and other pointer types. */

int memcmp(void *s1, void *s2, unsigned long n);

void *return_ptr(char *i) {
    return i + 3;
}

int check_char_ptr_argument(char *pointer, char expected_val) {
    return *pointer == expected_val;
}

int *return_void_ptr_as_int_ptr(void *pointer) {
    return pointer;
}

static double dbl5[5];  // static backing for get_dbl_array (replaces malloc)

double *get_dbl_array(unsigned long n) {
    return dbl5;
}

void set_doubles(double *array, unsigned long n, double d) {
    for (unsigned long i = 0; i < n; i = i + 1) {
        array[i] = d;
    }
    return;
}

void *return_dbl_ptr_as_void_ptr(double *ptr) {
    return ptr;
}

int main(void) {
    static long fourw;  // one word of static storage (replaces malloc(4))
    void *four_bytes = &fourw;

    int *int_ptr = four_bytes;

    // BESM-6: write through the char view and read it back (the original wrote an
    // int -1 and read byte #0 through char *, which depends on x86 byte layout).
    char *char_ptr = four_bytes;
    *char_ptr = 100;

    if (!check_char_ptr_argument(four_bytes, 100)) {
        return 1;
    }

    if (return_void_ptr_as_int_ptr(four_bytes) != int_ptr) {
        return 2;
    }

    double *dbl_ptr = four_bytes;
    int (*complicated_ptr)[3][2][5] = four_bytes;
    long *long_ptr = four_bytes;
    if (dbl_ptr != four_bytes || complicated_ptr != four_bytes || long_ptr != four_bytes) {
        return 3;
    }

    double *dbl_array = get_dbl_array(5);

    void *void_array = dbl_array;

    set_doubles(void_array, 5, 4.0);
    if (dbl_array[3] != 4.0) {
        return 4;
    }

    if (return_dbl_ptr_as_void_ptr(dbl_array) != void_array) {
        return 5;
    }

    void *some_other_ptr = 0;

    some_other_ptr = dbl_array;
    if (some_other_ptr != void_array) {
        return 6;
    }

    some_other_ptr = &some_other_ptr;
    if (some_other_ptr == void_array) {
        return 7;
    }

    complicated_ptr = 0;
    some_other_ptr = complicated_ptr;
    if (some_other_ptr) {
        return 8;
    }

    static long lpa0, lpa1, lpa2;  // static storage replaces three malloc(sizeof(long))
    long *long_ptr_array[3] = { &lpa0, &lpa1, &lpa2 };

    *long_ptr_array[0] = 100l;
    *long_ptr_array[1] = 200l;
    *long_ptr_array[2] = 300l;
    long sum = (*long_ptr_array[0] + *long_ptr_array[1] + *long_ptr_array[2]);
    if (sum != 600l) {
        return 9;
    }

    long arr1[3] = {1, 2, 3};
    long arr2[3] = {1, 2, 3};
    long arr3[3] = {1, 2, 4};
    if (memcmp(arr1, arr2, sizeof arr1) != 0) {
        return 10;
    }
    if (memcmp(arr1, arr3, sizeof arr2) >= 0) {
        return 11;
    }
    return 0;
}
