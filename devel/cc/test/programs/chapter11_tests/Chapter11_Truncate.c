int truncate(long l, int expected) {
    int result = (int) l;
    return (result == expected);
}

int main(void)
{
    if (!truncate(10l, 10)) {
        return 1;
    }
    if (!truncate(-10l, -10)) {
        return 2;
    }
    if (!truncate(17179869189l, // 2^34 + 5
                  5)) {
        return 3;
    }
    if (!truncate(-17179869179l, // (-2^34) + 5
                  5l)) {
        return 4;
    }
    int i = (int)17179869189l; // 2^34 + 5
    if (i != 5)
        return 5;
    return 0;
}
