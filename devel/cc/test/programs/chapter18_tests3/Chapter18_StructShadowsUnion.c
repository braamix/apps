struct s {int a; int b;};
union u {int i; unsigned int u;};

int main(void) {
    struct s my_struct = {12, 13};
    {
        union u storage;
        union u *ptr = &storage;
        ptr->i = 10;
        if (ptr->u != 10) {
            return 1; // fail
        }
        if (my_struct.b != 13) {
            return 2; // fail
        }
    }

    return 0; // success
}

