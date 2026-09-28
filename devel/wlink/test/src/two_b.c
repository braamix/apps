// libtwo.a: pulled by libone.a, and needs libone.a again.
const char *one_c(void);

const char *two_b(void)
{
    return one_c();
}
