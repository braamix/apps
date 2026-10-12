/* Test returning struct pointers from functions
 * and using struct pointers returned from functions
 * */

// define some struct types
struct inner {
    double d;
    int i;
};

struct outer {
    char a;
    char b;
    struct inner substruct;
};

struct outermost {
    int i;
    struct outer *nested_ptr;
    struct outer nested_struct;
};

// declare some functions that return pointers to structs
struct inner *mk_inner(int seed);
struct outer *mk_outer(int seed);
struct outermost *mk_outmost(int seed);
/* Test returning struct pointers from functions
 * and using struct pointers returned from functions
 * */


// case 1: use a struct pointer returned from a function call
int t_getptr(void) {
    struct inner *inner_ptr = mk_inner(11);

    if (inner_ptr->d != 11 || inner_ptr->i != 11) {
        return 0;
    }

    // assign struct pointer to member
    struct outermost o = {0, 0, {0, 0, {0, 0}}};
    o.nested_ptr = mk_outer(20);
    if (o.nested_ptr->a != 20 || o.nested_ptr->b != 21 ||
        o.nested_ptr->substruct.d != 22 || o.nested_ptr->substruct.i != 23) {
        return 0;
    }

    return 1;  // success
}

// case 2: apply member access operations to funcall expression
int t_getmem(void) {
    if (mk_inner(2)->d != 2) {
        return 0;
    }

    if (mk_outer(2)->substruct.d != 4) {
        return 0;
    }

    if (mk_outmost(0)->nested_ptr->a != 1) {
        return 0;
    }

    return 1;  // success
}

// case 3: update static structure member through pointer returned by funcall
// f()->member = val
struct outer *get_static_struct_ptr(void) {
    static struct outer s;
    return &s;
}

int t_updmem(void) {
    get_static_struct_ptr()->a = 10;
    get_static_struct_ptr()->substruct.d = 20.0;

    struct outer *ptr = get_static_struct_ptr();
    if (ptr->a != 10 || ptr->substruct.d != 20.0) {
        return 0;
    }

    return 1;  // success
}

// case 4: update whole structure member through pointer returned by funcall
int t_updnst(void) {
    struct inner small = {12.0, 13};
    get_static_struct_ptr()->substruct = small;
    if (get_static_struct_ptr()->substruct.d != 12.0) {
        return 0;
    }

    if (get_static_struct_ptr()->substruct.i != 13) {
        return 0;
    }

    return 1;  // success
}

int main(void) {
    if (!t_getptr()) {
        return 1;
    }

    if (!t_getmem()) {
        return 2;
    }

    if (!t_updmem()) {
        return 3;
    }

    if (!t_updnst()) {
        return 4;
    }

    return 0;
}
/* Test returning struct pointers from functions
 * and using struct pointers returned from functions
 * */


// define some functions that return pointers to structs
struct inner *mk_inner(int seed) {
    static struct inner msi;  // static storage replaces malloc
    struct inner *ptr = &msi;
    ptr->d = seed;
    ptr->i = seed;
    return ptr;
}

struct outer *mk_outer(int seed) {
    static struct outer mso;  // static storage replaces malloc
    struct outer *ptr = &mso;
    ptr->a = seed;
    ptr->b = seed + 1;
    ptr->substruct.d = seed + 2;
    ptr->substruct.i = seed + 3;
    return ptr;
}

struct outermost *mk_outmost(int seed) {
    static struct outermost msm;  // static storage replaces malloc
    struct outermost *ptr = &msm;
    ptr->i = seed;
    ptr->nested_ptr = mk_outer(seed + 1);
    ptr->nested_struct.a = seed + 5;
    ptr->nested_struct.b = seed + 6;
    ptr->nested_struct.substruct.d = seed + 7;
    ptr->nested_struct.substruct.i = seed + 8;
    return ptr;
}

