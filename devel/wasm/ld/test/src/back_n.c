// libfxa_back.a: pulled by back_m.c, and refers back into it.
const char *back_f(void);

const char *back_h(void)
{
    return back_f();
}
