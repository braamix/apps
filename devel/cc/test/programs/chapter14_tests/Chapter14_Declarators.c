int return_3(void);
int(return_3(void));
int(return_3)(void);
int((return_3))(void)
{
    return 3;
}


long l = 100;
long *two_pointers(double val, double *ptr)
{
    *ptr = val;
    return &l;
}
long(*two_pointers(double val, double(*d)));
long *(two_pointers)(double val, double *(d));
long *(two_pointers)(double val, double(*(d)));

unsigned **pointers_to_pointers(int **p)
{
    static unsigned u;
    static unsigned *u_ptr;
    u_ptr = &u;
    u = **p;
    return &u_ptr;
}
unsigned(**(pointers_to_pointers(int *(*p))));
unsigned *(*pointers_to_pointers(int(**p)));
unsigned(*(*((pointers_to_pointers)(int(*(*(p)))))));

int main(void)
{
    int i = 0;
    int(*i_ptr) = &i;
    int(**ptr_to_iptr) = &i_ptr;

    double(d1) = 0.0;
    double d2 = 10.0;

    double *(d_ptr) = &d1;

    long(*(l_ptr));

    unsigned *(*(ptr_to_uptr));

    i = return_3();
    if (i != 3)
        return 1;

    if (*i_ptr != 3) {
        return 2;
    }

    l_ptr = two_pointers(d2, d_ptr);
    if (l_ptr != &l) {
        return 3;
    }

    if (*l_ptr != 100) {
        return 4;
    }

    if (*d_ptr != 10.0) {
        return 5;
    }

    if (d1 != 10.0) {
        return 6;
    }


    ptr_to_uptr = pointers_to_pointers(ptr_to_iptr);

    if (**ptr_to_uptr != 3) {
        return 7;
    }

    return 0;
}
