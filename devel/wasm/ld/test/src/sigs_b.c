// The definitions sigs_a.c calls with other signatures, and a caller that
// agrees with them.
int sig_one(int a, int b)
{
    return a + b;
}

int sig_two(int a, int b)
{
    return a - b;
}

int sig_three(int a)
{
    return a * 3;
}

int sig_right(void)
{
    return sig_one(40, 2) + sig_two(1, 1) + sig_three(0);
}
