void their_values(long long *i, long double *f);
int main(void)
{
    long long oi[NI], ti[NI];
    long double of[NF], tf[NF];
    our_values(oi, of);
    their_values(ti, tf);
    for (int k = 0; k < NI; k++)
        if (oi[k] != ti[k])
            return 1 + k;
    for (int k = 0; k < NF; k++)
        if (of[k] != tf[k])
            return 100 + k;
    return 0;
}
