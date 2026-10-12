/* Verify that sizeof produces correct results for various expressions of
 * structure type. This is almost identical of sizeof_type except we're applying
 * sizeof to expressions and not just type specifiers.
 * This also tests that we correctly infer the types of expressions w/ structure
 * type
 * */

// struct declarations for size/layout tests

struct eight_bytes {
    int i;   // bytes 0-3
    char c;  // byte 4
             // 3 more bytes of padding to make size a multiple of 4
};

struct two_bytes {
    char arr[2];  // bytes 0-1
                  // no padding
};

struct three_bytes {
    char arr[3];  // bytes 0-2
                  // no padding
};

struct sixteen_bytes {
    struct eight_bytes eight;  // bytes 0-7
    struct two_bytes two;      // bytes 8-9
    struct three_bytes three;  // bytes 10-12
    // 3 bytes of padding to make size a multiple of 4  (i.e. 16 bytes)
    // b/c struct eightbyte is 4 byte-aligned)
};

struct seven_bytes {
    struct two_bytes two;      // bytes 0-1
    struct three_bytes three;  // bytes 2-4
    struct two_bytes two2;     // bytes 5-6
};                             // total size is 7 bytes

struct twentyfour_bytes {
    struct seven_bytes seven;  // bytes 0-6
    // 1 byte padding to make next member four-byte aligned
    struct sixteen_bytes sixteen;  // bytes 8-24 (four-byte aligned)
};

struct twenty_bytes {
    struct sixteen_bytes sixteen;  // bytes 0-15
    struct two_bytes two;          // bytes 16-17
    // 2 bytes padding to make the whole struct four-byte aligned
};  // 20 bytes b/c it's four-byte aligned

struct wonky {
    char arr[19];
};  // 19 bytes w/ no padding

struct internal_padding {
    char c;
    // 7 bytes of padding so next member is eight byte-aligned
    double d;
};  // 16 bytes total

struct contains_struct_array {
    char c;  // byte 0
    // 3 bytes padding so next member is 4 byte-aligned
    struct eight_bytes struct_array[3];  // bytes 4-27
};                                       // 28 bytes total

struct twenty_bytes *get_twentybyte_ptr(void) {
    return 0;
}

int main(void) {
    // validate the size of every type in struct_sizes.h

    struct contains_struct_array arr_struct;

    if (sizeof arr_struct.struct_array[2] !=
        8) {  // elements of struct_array have type struct eight_bytes
        return 1;
    }

    static struct twentyfour_bytes twentyfour;
    if (sizeof twentyfour.seven.two2 != 2) {
        return 2;
    }

    if (sizeof get_twentybyte_ptr()->sixteen.three != 3) {
        return 3;
    }

    if (sizeof get_twentybyte_ptr()->sixteen != 16) {
        return 4;
    }

    if (sizeof twentyfour.seven != 7) {
        return 5;
    }

    if (sizeof twentyfour != 24) {
        return 6;
    }

    if (sizeof *get_twentybyte_ptr() != 20) {
        return 7;
    }

    if (sizeof *((struct wonky *)0) != 19) {
        return 8;
    }

    extern struct internal_padding struct_array[4];
    if (sizeof struct_array[0] != 16) {
        return 9;
    }

    if (sizeof arr_struct != 28) {
        return 10;
    }

    if (sizeof struct_array != 64) {
        return 11;
    }

    // make sure arr_struct.struct_array doesn't undergo array decay here
    if (sizeof arr_struct.struct_array != 24) {
        return 12;
    }

    return 0;  // success
}

