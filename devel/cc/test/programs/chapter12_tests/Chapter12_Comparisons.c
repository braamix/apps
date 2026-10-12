unsigned int small_uint = 100u;
unsigned int large_uint = 4294967294u; // interpreted as a signed int, this would be -2

unsigned long small_ulong = 100ul;
unsigned long large_ulong = 4294967294ul; // this would have the same value as a signed long

int main(void) {
    // compare unsigned ints (result would be different if interpreted as signed)

    /* False comparisons */
    if (large_uint < small_uint)
        return 1;
    if (large_uint <= small_uint)
        return 2;
    if (small_uint >= large_uint)
        return 3;
    if (small_uint > large_uint)
        return 4;
    /* True comparisons */
    if (!(small_uint <= large_uint))
        return 5;
    if (!(small_uint < large_uint))
        return 6;
    if (!(large_uint > small_uint))
        return 7;
    if (!(large_uint >= small_uint))
        return 8;

    // compare unsigned longs (result would be the same if interpreted as signed)
    /* False comparisons: */
    if (large_ulong < small_ulong)
        return 9;
    if (large_ulong <= small_ulong)
        return 10;
    if (small_ulong >= large_ulong)
        return 11;
    if (small_ulong > large_ulong)
        return 12;
    /* True comparisons */
    if (!(small_ulong <= large_ulong))
        return 13;
    if (!(small_ulong < large_ulong))
        return 14;
    if (!(large_ulong > small_ulong))
        return 15;
    if (!(large_ulong >= small_ulong))
        return 16;

    return 0;
}
