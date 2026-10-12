/* Test for accessing the members in a return value of structure type */
struct inner {
    char x;
    long y;
};

struct outer {
    double d;
    struct inner *ptr;
    struct inner s;
};

struct inner return_small_struct(void);
struct outer return_nested_struct(void);
/* Test for accessing the members in a return value of structure type */

int main(void) {
    // get member in a non-nested struct
    if (return_small_struct().y != 102) {
        return 1;
    }

    // get members in nested struct
    if (return_nested_struct().d != 2.0 || return_nested_struct().s.x != 10 ||
        return_nested_struct().s.y != 11) {
        return 3;
    }

    // get members thru pointer in nested struct
    if (return_nested_struct().ptr->x != 12 ||
        return_nested_struct().ptr->y != 13) {
        return 4;
    }

    // update members through pointer in nested struct
    return_nested_struct().ptr->x = 70;
    return_nested_struct().ptr->y = 71;

    // validate updated values
    if (return_nested_struct().ptr->x != 70 ||
        return_nested_struct().ptr->y != 71) {
        return 5;
    }

    return 0;  // success
}
/* Test for accessing the members in a return value of structure type */

struct inner return_small_struct(void) {
    struct inner i = {101, 102};
    return i;
}

struct outer return_nested_struct(void) {
    static struct outer ret = {2.0, 0, {10, 11}};

    // on first call to this function, initialize ret.ptr (static storage replaces calloc)
    static struct inner ri;
    if (!ret.ptr) {
        ret.ptr = &ri;
        ret.ptr->x = 12;
        ret.ptr->y = 13;
    }

    return ret;
}

