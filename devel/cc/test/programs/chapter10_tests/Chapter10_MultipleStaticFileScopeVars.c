static int foo;

int main(void) {
    return foo;
}

extern int foo;

static int foo = 4;
