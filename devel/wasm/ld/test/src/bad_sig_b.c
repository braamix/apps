// A direct call to the same function with another signature: with no
// definition to decide between them, neither can be imported.
int sig_fn(int a, int b);

int sig_other(void)
{
    return sig_fn(1, 2);
}
