long *get_pointer(void);
int set_pointer(long *new_ptr);

static long private_long = 100l;

int main(void) {
    long *initial_ptr = get_pointer();
    if (initial_ptr) {
        return 1;
    }

    set_pointer(&private_long);

    long *new_ptr = get_pointer();
    if (initial_ptr == new_ptr) {
        return 2;
    }

    if (*new_ptr != 100l) {
        return 3;
    }

    if (new_ptr != &private_long) {
        return 4;
    }

    set_pointer(0);

    if (get_pointer()) {
        return 5;
    }

    if (new_ptr != &private_long) {
        return 6;
    }

    return 0;
}

static long *long_ptr;

long *get_pointer(void) {
    return long_ptr;
}

int set_pointer(long *new_ptr) {
    long_ptr = new_ptr;
    return 0;
}
