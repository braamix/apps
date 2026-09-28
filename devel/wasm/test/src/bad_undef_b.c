// A second file that references what bad_undef_a.c does.
extern int nowhere(void);

int nowhere_b(void)
{
    return nowhere();
}
