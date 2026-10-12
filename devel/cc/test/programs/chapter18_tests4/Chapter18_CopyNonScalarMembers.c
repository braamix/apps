union simple {
    int i;
    long l;
    char c;
    unsigned char uc_arr[3];
};

union has_union {
    double d;
    union simple u;
    union simple *u_ptr;
};

struct simple_struct {
    long l;
    double d;
    unsigned int u;
};

union has_struct {
    long l;
    struct simple_struct s;
};

struct struct_with_union {
    union simple u;
    unsigned long ul;
};

union complex_union {
    double d_arr[2];
    struct struct_with_union s;
    union has_union *u_ptr;
};
// Read and assign to non-scalar union members

int test_dot(void) {
    // Test reading/writing whole nested unions/structs w/ . operator
    // first, assign a union to a struct member
    struct struct_with_union my_struct = { {0}, 100000l };
    union simple my_simple_union;
    my_simple_union.l = -1;
    my_struct.u = my_simple_union;

    // now assign to a union mebmer of struct type
    static union complex_union my_union;
    my_union.s = my_struct;

    // validate what we have so far
    if (my_struct.ul != 100000l || my_struct.u.l != -1) {
        return 0; // fail
    }

    if (my_union.s.ul != 100000l) {
        return 0; // fail
    }

    // now copy whole structs/unions from members
    my_union.s.u.i = 45;
    // copy simple_union sub-object from my_union into my_simple_union
    my_simple_union = my_union.s.u;
    if (my_simple_union.i != 45) {
        return 0; // fail
    }

    // copy struct sub-object from my_union into another (static) variable
    static struct struct_with_union another_struct;
    another_struct = my_union.s;
    if (another_struct.ul != 100000l || another_struct.u.i != 45) {
        return 0; // fail
    }

    return 1; // success
}

int test_arrow(void) {
    // static objects in place of heap allocations
    static union complex_union cu_store;
    static union has_union hu_store;
    static union simple su_store;
    union complex_union* my_union_ptr = &cu_store;
    my_union_ptr->u_ptr = &hu_store;
    my_union_ptr->u_ptr->u_ptr = &su_store;
    my_union_ptr->u_ptr->u_ptr->i = 987654321;

    // read thru arrow to assign
    union has_union another_union = *my_union_ptr->u_ptr;

    // compare pointers & pointers' dereferenced values
    if (another_union.u_ptr != my_union_ptr->u_ptr->u_ptr || another_union.u_ptr->c != my_union_ptr->u_ptr->u_ptr->c) {
        return 0; // fail
    }

    // define another object to assign through arrow
    union simple small_union = { -9999 };
    my_union_ptr->u_ptr->u = small_union;
    if (my_union_ptr->u_ptr->u.i != -9999) {
        return 0; // fail
    }

    return 1; // success
}

int main(void) {
    if (!test_dot()) {
        return 1;
    }

    if (!test_arrow()) {
        return 2;
    }

    return 0; // success
}

