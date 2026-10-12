/* Test working with a structure whose type is completed in the library but not
 * the client; this is a common idiom for hiding a library's implementation
 * details */

struct s;  // declare a type but don't define it

// declare some functions for getting/using pointers to this type
struct s *create_struct(int i, double d, char *s);
void increment_struct(struct s *src_ptr);

int check_struct(struct s *ptr, int expected_i, double expected_d,
                 char *expected_s);
void print_struct_msg(struct s *ptr);
struct s *get_internal_struct(void);

// declare a variable with this type
extern struct s incomplete_var;

int main(void) {
    struct s *new_struct = create_struct(101, 102.0, "NEW STRUCT");

    struct s *internal_struct = get_internal_struct();

    // print initial message from all three structs
    print_struct_msg(new_struct);
    print_struct_msg(internal_struct);
    print_struct_msg(&incomplete_var);

    // modify some structs
    increment_struct(new_struct);
    increment_struct(&incomplete_var);

    // check values
    if (!check_struct(new_struct, 102, 103.0, "NEW STRUCT")) {
        return 1;
    }

    if (!check_struct(&incomplete_var, 4, 5.0, "GLOBAL STRUCT")) {
        return 2;
    }

    return 0;  // success (assuming stdout is also correct)
}
/* Test working with a structure whose type is completed in the library but not
 * the client; this is a common idiom for hiding a library's implementation
 * details */


// library functions
int strcmp(char *s1, char *s2);
int puts(char *s);

struct s {
    int member1;
    double member2;
    char *member3;
};

// make a struct
struct s *create_struct(int i, double d, char *s) {
    static struct s cs_obj;  // static storage replaces malloc
    struct s *ptr = &cs_obj;
    ptr->member1 = i;
    ptr->member2 = d;
    ptr->member3 = s;
    return ptr;
}

// modify a struct
void increment_struct(struct s *ptr) {
    ptr->member1 = ptr->member1 + 1;
    ptr->member2 = ptr->member2 + 1;
    ptr->member3 = ptr->member3;
}

// read struct members
int check_struct(struct s *ptr, int expected_i, double expected_d,
                 char *expected_s) {
    if (ptr->member1 != expected_i) {
        return 0;
    }
    if (ptr->member2 != expected_d) {
        return 0;
    }
    if (strcmp(ptr->member3, expected_s)) {
        return 0;
    }

    return 1;  // success
}

void print_struct_msg(struct s *ptr) {
    puts(ptr->member3);
}

// define a struct s that isn't visible in the other translation unit
static struct s internal = {1, 2.0, "STATIC STRUCT"};

struct s *get_internal_struct(void) {
    return &internal;
}

// define struct that is visible in other translation unit
// (although its members aren't accessible)
struct s incomplete_var = {3, 4.0, "GLOBAL STRUCT"};

