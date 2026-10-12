/* Verify that sizeof produces correct results for various structure types (and
 * arrays of structs) */

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

int main(void) {
    // validate the size of every type in struct_sizes.h

    if (sizeof(struct eight_bytes) != 8) {
        return 1;
    }

    if (sizeof(struct two_bytes) != 2) {
        return 2;
    }

    if (sizeof(struct three_bytes) != 3) {
        return 3;
    }

    if (sizeof(struct sixteen_bytes) != 16) {
        return 4;
    }

    if (sizeof(struct seven_bytes) != 7) {
        return 5;
    }

    if (sizeof(struct twentyfour_bytes) != 24) {
        return 6;
    }

    if (sizeof(struct twenty_bytes) != 20) {
        return 7;
    }

    if (sizeof(struct wonky) != 19) {
        return 8;
    }

    if (sizeof(struct internal_padding) != 16) {
        return 9;
    }

    if (sizeof(struct contains_struct_array) != 28) {
        return 10;
    }

    if (sizeof(struct internal_padding[4]) != 64) {
        return 11;
    }

    if (sizeof(struct wonky[2]) != 38) {
        return 12;
    }

    return 0;  // success
}

