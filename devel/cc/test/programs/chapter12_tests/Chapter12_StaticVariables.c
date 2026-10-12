static unsigned long x = 281474976710600ul; // 2^48 - 56

unsigned long zero_long;
unsigned zero_int;

int main(void)
{
    if (x != 281474976710600ul)
        return 0;
    x = x + 10;
    if (x != 281474976710610ul)
        return 0;
    if (zero_long || zero_int)
        return 0;
    return 1;
}
