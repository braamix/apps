// libone.a: pulled only by libtwo.a, which comes after it on the line.
const char *one_c(void)
{
    return "one_a -> two_b -> one_c";
}
