/* Test that our typechecker can handle valid declarations and expressions
 * involving incomplete union types
 * */


int puts(char *s);

 // Test 1: you can declare a function that accepts/returns incomplete
 // union types
union never_used;
union never_used incomplete_fun(union never_used x);

// test 2: declare an incomplete union type, then complete it.
// BESM-6: block-scope tag *definitions* are unsupported, so this type is
// forward-declared and completed at file scope instead of inside the function.
union bsu;  // declare incomplete union type
union bsu {
    long x;
    char y;
};  // complete the type

int test_block_scope_forward_decl(void) {
    union bsu* u_ptr = 0;  // pointer to the (now complete) union type

    union bsu val = { 100000000l };
    u_ptr = &val;
    if (u_ptr->x != 100000000l || u_ptr->y != 0) {
        return 0; // fail
    }

    return 1;  // success
}

// test 3: you can pass and return pointers to incomplete union types
union opaque_union;

union opaque_union* use_union_pointers(union opaque_union* param) {
    if (param == 0) {
        puts("NULL POINTER");
    }

    return 0;
}

int test_use_incomplete_union_pointers(void) {
    // define a couple of pointers to this type
    // (distinct zeroed static ints back the incomplete-type pointers)
    static int ou1, ou2;
    union opaque_union* ptr1 = (union opaque_union*)&ou1;
    union opaque_union* ptr2 = (union opaque_union*)&ou2;

    // can cast to char * and inspect; this is well-defined
    // and all bits should be 0 since we used calloc
    char* ptr1_bytes = (char*)ptr1;
    if (ptr1_bytes[0] || ptr1_bytes[1]) {
        return 0;
    }

    // can compare to 0 or each other
    if (ptr1 == 0 || ptr2 == 0 || ptr1 == ptr2) {
        return 0;
    }

    // can use them in conditionals
    static int flse = 0;
    union opaque_union* ptr3 = flse ? ptr1 : ptr2;
    if (ptr3 != ptr2) {
        return 0;
    }

    // can pass them as parameters
    if (use_union_pointers(ptr3)) {
        return 0;
    }

    return 1;  // success
}

int main(void) {
    if (!test_block_scope_forward_decl()) {
        return 1; // fail
    }

    if (!test_use_incomplete_union_pointers()) {
        return 2; // fail
    }

    return 0; // success
}

