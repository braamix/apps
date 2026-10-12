int *return_pointer(void) {
    static int var = 10;
    return &var;
}

int one = 1;

int main(void) {
    int val = 100;
    int *ptr_var = &val;

    if (*return_pointer() != 10) {
        return 1;
    }

    if (*(one ? return_pointer() : ptr_var) != 10)
        return 2;

    if (*(one - 1 ? return_pointer() : ptr_var) != 100) {
        return 3;
    }


    int *ptr_to_one = &one;
    if (*(ptr_var = ptr_to_one) != 1) {
        return 4;
    }

    *return_pointer() = 20;
    *(one ? ptr_var : return_pointer()) = 30;

    if (*return_pointer() != 20) {
        return 5;
    }
    if (*ptr_var != 30) {
        return 6;
    }
    if (one != 30) {
        return 7;
    }

    return 0;
}
