unsigned int w = 4294967295U;
int x = 10;
unsigned int y = 4294967295U;
double *dbl_ptr;

long modify_ptr(long *new_ptr) {
    static long *p;
    if (new_ptr)
    {
        p = new_ptr;
    }
    return *p;
}


int increment_ptr(void)
{
    *dbl_ptr = *dbl_ptr + 5.0;
    return 0;
}

int main(void) {

    int *pointer_to_static = &x;
    x = 20;
    if (*pointer_to_static != 20) {
        return 1;
    }

    *pointer_to_static = 100;

    if (x != 100) {
        return 2;
    }
    if (w != 4294967295U) {
        return 3;
    }
    if (y != 4294967295U) {
        return 4;
    }
    if (dbl_ptr) {
        return 5;
    }

    long l = 1000l;

    if (modify_ptr(&l) != 1000l) {
        return 6;
    }

    l = -1;
    if (modify_ptr(0) != l) {
        return 7;
    }

    double d = 10.0;
    dbl_ptr = &d;
    increment_ptr();
    if (*dbl_ptr != 15) {
        return 8;
    }

    return 0;
}
