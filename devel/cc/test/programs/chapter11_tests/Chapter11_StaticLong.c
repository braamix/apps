static long foo = 4294967290l;

int main(void)
{
    if (foo + 5l == 4294967295l)
    {
        foo = 1099511627770l;
        if (foo == 1099511627770l)
            return 1;
    }
    return 0;
}
