// libfxa_back.a: back_h is referred to before back_f is defined, so
// back_n.c's reference to back_f arrives while this member is loading.
const char *back_h(void);

const char *back_g(void)
{
    return back_h();
}

const char *back_f(void)
{
    return "p -> g -> h -> f";
}
