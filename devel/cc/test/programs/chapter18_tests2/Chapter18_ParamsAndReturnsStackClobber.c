/* Test that returning a struct doesn't clobber the stack.
 * This is most likely when we're returning structs in memory, but test other
 * types of structs too.
 * To test this, we store some bytes on the stack, call a function that returns
 * the struct, then validate that those bytes haven't changed. In the functions
 * whose stacks we validate, we don't store any values on the stack except the
 * bytes to validate and the return value. This ensures that if the return
 * value clobbers any other bytes on the stack, we'll detect it. This is a
 * similar technique to
 * chapter_18/valid/no_structure_parameters/struct_copy/stack_clobber.c
 * This test assumes structures are allocated on the stack in the same order
 * they're declared/initialized (otherwise clobbers may overwrite stack padding
 * instead of data that we validate, and go undetected).
 */


int strcmp(char *s1, char *s2);
#include <stdlib.h>

struct stack_bytes {
    char bytes[16];
};

// we copy bytes from the stack to here, then validate them
static struct stack_bytes to_validate;

// use this to validate to_validate after copying bytes from stack to it
void vsb(int code) {
    if (strcmp(to_validate.bytes, "EFGHIJKLMNOPQRS")) {
        exit(code);
    }
    return;
}

// test case 1: return a struct in a general-purpose register
struct one_int_reg {
    char cs[7];
};

struct one_int_reg ret1(void) {
    struct one_int_reg retval = {{0, 0, 0, 0, 0, 0, 0}};
    return retval;
}

static struct one_int_reg one_int_struct;
void vck1(int code) {
    for (int i = 0; i < 7; i = i + 1) {
        if (one_int_struct.cs[i]) {
            exit(code);
        }
    }
}

int tc1(void) {
    // write some bytes to the stack
    struct stack_bytes bytes = {"EFGHIJKLMNOPQRS"};

    // call a function that returns a one-int struct
    // copy it to a static variable so we can validate it later
    // without putting more temporary variables on the satck
    one_int_struct = ret1();

    // assigning a variable doesn't produce any temporary values
    to_validate = bytes;

    // this funcall doesn't require temporary values on the stack
    // b/c its arg is just an int(not a more complex expression)
    // and its return type
    vsb(1);

    /// validate the static struct we copied the return val into earlier
    vck1(2);
    return 0;
}

// test case 2: return a struct in two general-purpose registers
struct two_int_regs {
    char cs[15];
};

struct two_int_regs ret2(void) {
    struct two_int_regs retval = {
        {20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34}};
    return retval;
}

static struct two_int_regs two_int_struct;
void vck2(int code) {
    for (int i = 0; i < 15; i = i + 1)
        if (two_int_struct.cs[i] != i + 20) {
            exit(code);
        }
}

int tc2(void) {
    // write some bytes to the stack
    struct stack_bytes bytes = {"EFGHIJKLMNOPQRS"};

    two_int_struct = ret2();

    // assigning a variable doesn't produce any temporary values
    to_validate = bytes;

    // validate stack
    vsb(3);

    /// validate returned struct
    vck2(4);
    return 0;
}

// test case 3: return a struct in one XMM register
struct one_xmm_reg {
    double d;
};

struct one_xmm_reg ret3(void) {
    struct one_xmm_reg retval = {234.5};
    return retval;
}

static struct one_xmm_reg one_double_struct;
void vck3(int code) {
    if (one_double_struct.d != 234.5) {
        exit(code);
    }
}

int tc3(void) {
    // write some bytes to the stack
    struct stack_bytes bytes = {"EFGHIJKLMNOPQRS"};

    one_double_struct = ret3();

    // assigning a variable doesn't produce any temporary values
    to_validate = bytes;

    // validate stack
    vsb(5);

    /// validate returned struct
    vck3(6);
    return 0;
}

// test case 4: return a struct in two XMM registers
struct two_xmm_regs {
    double d1;
    double d2;
};

struct two_xmm_regs ret4(void) {
    struct two_xmm_regs retval = {234.5, 678.25};
    return retval;
}

static struct two_xmm_regs two_doubles_struct;
void vck4(int code) {
    if (two_doubles_struct.d1 != 234.5 || two_doubles_struct.d2 != 678.25) {
        exit(code);
    }
}

int tc4(void) {
    // write some bytes to the stack
    struct stack_bytes bytes = {"EFGHIJKLMNOPQRS"};

    two_doubles_struct = ret4();

    // assigning a variable doesn't produce any temporary values
    to_validate = bytes;

    // validate stack
    vsb(7);

    /// validate returned struct
    vck4(8);
    return 0;
}

// test case 5: return a stuct in general-purpose and XMM registers

struct int_and_xmm {
    char c;
    double d;
};

struct int_and_xmm ret5(void) {
    struct int_and_xmm retval = {125, 678.25};
    return retval;
}

static struct int_and_xmm mixed_struct;
void vck5(int code) {
    if (mixed_struct.c != 125 || mixed_struct.d != 678.25) {
        exit(code);
    }
}

int tc5(void) {
    // write some bytes to the stack
    struct stack_bytes bytes = {"EFGHIJKLMNOPQRS"};

    mixed_struct = ret5();

    // assigning a variable doesn't produce any temporary values
    to_validate = bytes;

    // validate stack
    vsb(9);

    /// validate returned struct
    vck5(10);
    return 0;
}

// test case 6: return a struct on the stack
struct stack {
    char cs[28];
};

struct stack ret6(void) {
    struct stack retval = {{90,  91,  92,  93,  94,  95,  96,  97,  98,  99,
                            100, 101, 102, 103, 104, 105, 106, 107, 108, 109,
                            110, 111, 112, 113, 114, 115, 116, 117}};
    return retval;
}

static struct stack stack_struct;
void vck6(int code) {
    for (int i = 0; i < 28; i = i + 1) {
        if (stack_struct.cs[i] != i + 90) {
            exit(code);
        }
    }
}

int tc6(void) {
    // write some bytes to the stack
    struct stack_bytes bytes = {"EFGHIJKLMNOPQRS"};

    stack_struct = ret6();

    // assigning a variable doesn't produce any temporary values
    to_validate = bytes;

    // validate stack
    vsb(11);

    /// validate returned struct
    vck6(12);
    return 0;
}

// test case 7: return an irregularly-slized struct on the stack
struct stack_irregular {
    char cs[19];
};

struct stack_irregular ret7(void) {
    struct stack_irregular retval = {{70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
                                      80, 81, 82, 83, 84, 85, 86, 87, 88}};
    return retval;
}

static struct stack_irregular irregular_stack_struct;
void vck7(int code) {
    for (int i = 0; i < 19; i = i + 1) {
        if (irregular_stack_struct.cs[i] != i + 70) {
            exit(code);
        }
    }
}

int tc7(void) {
    // write some bytes to the stack
    struct stack_bytes bytes = {"EFGHIJKLMNOPQRS"};

    irregular_stack_struct = ret7();

    // assigning a variable doesn't produce any temporary values
    to_validate = bytes;

    // validate stack
    vsb(13);

    /// validate returned struct
    vck7(14);
    return 0;
}

int main(void) {
    tc1();
    tc2();
    tc3();
    tc4();
    tc5();
    tc6();
    tc7();
    return 0;
}

