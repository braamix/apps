double *d = 0l;
int *i = 0ul;
int *i2 = 0u;

int expect_null_param(int *val)
{
    return (val == 0ul);
}

long *return_null_ptr(void)
{
    return 0;
}

int main(void)
{
    int x = 10;
    int *ptr = &x;

    if (d) {
        return 1;
    }

    if (i) {
        return 2;
    }
    if (i2) {
        return 3;
    }

    ptr = 0ul;
    if (ptr) {
        return 4;
    }

    int *y = 0;
    if (y != 0)
        return 5;

    if (!expect_null_param(0)) {
        return 6;
    }

    long *null_ptr = return_null_ptr();
    if (null_ptr != 0) {
        return 7;
    }

    ptr = &x;
    int *ternary_result = 10 ? 0 : ptr;
    if (ternary_result) {
        return 8;
    }

    return 0;
}
