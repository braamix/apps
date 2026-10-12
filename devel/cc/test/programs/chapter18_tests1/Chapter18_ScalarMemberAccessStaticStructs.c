// Make sure members in static structures retain their values
// across multiple function invocations


int putchar(int ch);
int puts(char *s);

// BESM-6 adaptation: `struct s` is declared at file scope (the book declares it
// twice inside the two helpers).  A static local of a *block-local* struct type
// is unsupported: the static-local's type is resolved at translate time, after
// the block-scope tag has been purged from structtab.  Both helpers use the same
// {int a; int b;} layout, so a single shared file-scope tag is equivalent.
struct s {
    int a;
    int b;
};

// test that changes to static struct are retained across function calls
// do this by validating text written to stdout,
// instead of usual pattern of signifying success/failure with return value
void test_sl(int a, int b) {
    static struct s static_struct;
    if (!(static_struct.a || static_struct.b)) {
        puts("ZERO");
    } else {
        putchar(static_struct.a);
        putchar(static_struct.b);
        putchar('\n');
    }

    static_struct.a = a;
    static_struct.b = b;
}

// test that changes to struct made through static pointer are retained across
// function calls do this by validating text written to stdout
void test_slp(int a, int b) {
    static struct s *struct_ptr;
    static struct s backing;  // static storage replaces malloc (no heap)
    if (!struct_ptr) {
        struct_ptr = &backing;
    } else {
        putchar(struct_ptr->a);
        putchar(struct_ptr->b);
        putchar('\n');
    }

    struct_ptr->a = a;
    struct_ptr->b = b;
}

// test that changes to global struct are visible across function calls
struct global {
    char x;
    char y;
    char z;
};

struct global g;

void f1(void) {
    g.x = g.x + 1;
    g.y = g.y + 1;
    g.z = g.z + 1;
}

void f2(void) {
    putchar(g.x);
    putchar(g.y);
    putchar(g.z);
    putchar('\n');
}

void test_gs(void) {
    g.x = 'A';
    g.y = 'B';
    g.z = 'C';

    f1();
    f2();
    f1();
    f2();
}

// test that changes to global struct pointer are visible across function calls
struct global *g_ptr;
struct global g2;  // static backing replaces malloc (no heap)

void f3(void) {
    g_ptr->x = g_ptr->x + 1;
    g_ptr->y = g_ptr->y + 1;
    g_ptr->z = g_ptr->z + 1;
}

void f4(void) {
    putchar(g_ptr->x);
    putchar(g_ptr->y);
    putchar(g_ptr->z);
    putchar('\n');
}

void test_gsp(void) {
    g_ptr = &g;  // first, point to global struct from previous test
    f3();
    f4();
    f3();
    f4();
    // now point to a separate static struct instead
    g_ptr = &g2;
    g_ptr->x = 'A';
    g_ptr->y = 'B';
    g_ptr->z = 'C';
    f3();
    f4();
    f3();
    f4();
}

int main(void) {
    test_sl('M', 'N');
    test_sl('O', 'P');
    test_sl('!', '!');
    ;  // last one, won't be printed
    test_slp('W', 'X');
    test_slp('Y', 'Z');
    test_slp('!', '!');
    ;  // last one, won't be printed
    test_gs();
    test_gsp();
    return 0;
}

