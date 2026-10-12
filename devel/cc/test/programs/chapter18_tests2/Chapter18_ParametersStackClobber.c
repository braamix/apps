/* Test that passing structures as parameters doesn't clobber the stack.
 * To test this, we store some bytes to the stack, pass the struct, then
 * validate that those bytes haven't changed.
 * Our test functions don't store any values on the stack except the ones we
 * explicitly validate; e.g. they don't call functions that return values,
 * evaluate any expressions that undergo array decay (because the result of
 * GetAddr would be stored on the stack) or perform any other computations that
 * produce intermediate expressions. This ensures that if any value on the stack
 * is clobbered, we'll detect it.
 */


int strcmp(char *s1, char *s2);
#include <stdlib.h>

struct stack_bytes {
    char bytes[16];
};

// we copy bytes from the stack to here, then validate them
static struct stack_bytes to_validate;

// use this to validate to_validate after copying bytes from stack to it
void validate_stack_bytes(int code) {
    if (strcmp(to_validate.bytes, "EFGHIJKLMNOPQRS")) {
        exit(code);
    }
    return;
}

// test case 1: passing a struct holding four-byte int
struct one_longword {
    int i;
};

void take_longword(struct one_longword s, int code) {
    if (s.i != 10) {
        exit(code);
    }
    return;
}

int pass_longword(void) {
    // write some bytes to the stack
    struct stack_bytes bytes = {"EFGHIJKLMNOPQRS"};
    // make this static so it's not on the stack
    static struct one_longword my_var = {10};
    // this funcall doesn't require temporary values on the stack
    // b/c its args are just a variable and int (not more complex expressions)
    // and its return type is void
    take_longword(my_var, 1);

    // assigning a variable doesn't produce any temporary values
    to_validate = bytes;

    // this funcall doesn't require temporary values on the stack
    // b/c its arg is just an int(not a more complex expression)
    // and its return type
    validate_stack_bytes(2);
    return 0;
}

// test case #2: passing a struct holding an eight-byte int
struct one_quadword {
    long l;
};

void take_quadword(struct one_quadword s, int code) {
    if (s.l != 10) {
        exit(code);
    }
    return;
}

int pass_quadword(void) {
    // write some bytes to the stack
    struct stack_bytes bytes = {"EFGHIJKLMNOPQRS"};

    static struct one_quadword my_var = {10};
    take_quadword(my_var, 3);

    // validate stack
    to_validate = bytes;
    validate_stack_bytes(4);
    return 0;
}

// test case #3: passing a struct holding a double
struct one_double {
    double d;
};

void take_double(struct one_double s, int code) {
    if (s.d != 10) {
        exit(code);
    }
    return;
}

int pass_double(void) {
    // write some bytes to the stack
    struct stack_bytes bytes = {"EFGHIJKLMNOPQRS"};
    static struct one_double my_var = {10};
    take_double(my_var, 5);

    // validate stack
    to_validate = bytes;
    validate_stack_bytes(6);
    return 0;
}

// test case #4: passing a struct holding twelve bytes
struct twelve_bytes {
    char arr[12];
};

void take_twelve_bytes(struct twelve_bytes s, int code) {
    if (strcmp(s.arr, "ABCDEFGHIJK")) {
        exit(code);
    }
    return;
}

int pass_twelve_bytes(void) {
    struct stack_bytes bytes = {"EFGHIJKLMNOPQRS"};
    static struct twelve_bytes my_var = {"ABCDEFGHIJK"};
    take_twelve_bytes(my_var, 7);

    // validate stack
    to_validate = bytes;
    validate_stack_bytes(8);
    return 0;
}

// test case #5: passing a struct in memory
// make sure this is an even number of quadwords so we don't need to add stack
// padding
struct memory {
    char arr[32];
};

void take_struct_in_mem(struct memory s, int code) {
    if (strcmp(s.arr, "HERE'S THE THING: I'M A STRING.")) {
        exit(code);
    }
    return;
}

int pass_struct_in_mem(void) {
    struct stack_bytes bytes = {"EFGHIJKLMNOPQRS"};
    static struct memory my_var = {"HERE'S THE THING: I'M A STRING."};
    take_struct_in_mem(my_var, 9);

    // validate stack
    to_validate = bytes;
    validate_stack_bytes(10);
    return 0;
}

// test case #6: passing a 3-byte struct
struct irregular {
    char arr[3];
};

void take3(struct irregular s, int code) {
    if (strcmp(s.arr, "12")) {
        exit(code);
    }
    return;
}

int pass3(void) {
    struct stack_bytes bytes = {"EFGHIJKLMNOPQRS"};
    static struct irregular my_var = {"12"};
    take3(my_var, 11);

    // validate stack
    to_validate = bytes;
    validate_stack_bytes(12);
    return 0;
}

// test case #7: passing an irregularly-sized struct in memory
// make sure this is an even number of quadwords so we don't need to add stack
// padding
struct irregular_memory {
    char arr[27];
};

void take27(struct irregular_memory s, int code) {
    if (strcmp(s.arr, "THE QUICK BROWN FOX JUMPED")) {
        exit(code);
    }
    return;
}

int pass27(void) {
    struct stack_bytes bytes = {"EFGHIJKLMNOPQRS"};

    static struct irregular_memory my_var = {"THE QUICK BROWN FOX JUMPED"};
    take27(my_var, 13);

    // validate stack
    to_validate = bytes;
    validate_stack_bytes(14);
    return 0;
}

int main(void) {
    pass_longword();
    pass_quadword();
    pass_double();
    pass_twelve_bytes();
    pass_struct_in_mem();
    pass3();
    pass27();
    return 0;
}

