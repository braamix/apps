unsigned int u = 1152921506754330636l; // 2^60 + 2^31 + 12
int i = 2147483650u;
long l = 9223372036854775900u; // note: this has type unsigned long
long l2 = 2147483650u;
unsigned long ul = 4294967294u;
unsigned long ul2 = 9223372036854775798l;
int i2 = 9223372039002259606ul; // 2^63 + 2^31 + 150
unsigned ui2 = 9223372039002259606ul;

int main(void)
{
    if (u != 2147483660u)
        return 1;
    if (i != -2147483646)
        return 2;
    if (l != -9223372036854775716l)
        return 3;
    if (l2 != 2147483650l)
        return 4;
    if (ul != 4294967294ul)
        return 5;
    if (ul2 != 9223372036854775798ul)
        return 6;
    if (i2 != -2147483498)
        return 7;
    if (ui2 != 2147483798u)
        return 8;
    return 0;
}
