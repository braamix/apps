// Apply sizeof to a range of union types
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

// size is 11 bytes; no padding
union no_padding {
    char c;
    unsigned char uc;
    signed char arr[11];
};

// size is 12 bytes; take largest member (10 bytes)
// and pad to 4-byte alignment (b/c ui is 4-byte aligned)
union with_padding {
    signed char arr[10];
    unsigned int ui;
};

// size is 36 bytes
// arr1 is 24 bytes, 4-byte aligned
// arr2 is 33 bytes, 1-byte aligned
// round 33 up to multiple of 4 to get 36
union contains_array {
    union with_padding arr1[2];
    union no_padding arr[3];
};

// 8 bytes, no padding
union double_and_int {
    int i;
    double d;
};

// 20 bytes, 4-byte aligned
union contains_structs {
    struct wonky x; // 19 bytes, 1-byte aligned
    struct eight_bytes y; // 8 bytes, 4-byte aligned
};

int main(void) {
    if (sizeof(union no_padding) != 11) {
        return 1; // fail
    }

    if (sizeof(union with_padding) != 12) {
        return 2; // fail
    }

    if (sizeof(union contains_array) != 36) {
        return 3; // fail
    }

    if (sizeof(union double_and_int) != 8) {
        return 4; // fail
    }

    if (sizeof(union contains_structs) != 20) {
        return 5; // fail
    }

    // apply sizeof to some expressions with union type too
    union no_padding x = { 1 };
    union contains_array y = { {{{-1, 2}} }};
    union contains_structs* get_union_ptr(void);

    if (sizeof x != 11) {
        return 6; // fail
    }

    if (sizeof y.arr1 != 24) { // array of two union with_padding objects
        return 7; // fail
    }

    if (sizeof * get_union_ptr() != 20) {
        return 8; // fail
    }

    return 0; // success
}

union contains_structs* get_union_ptr(void) {
    // just return null pointer - okay b/c we never actually access this struct
    return 0;
}

