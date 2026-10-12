// Test bitwise compound assignment operators with character types

int main(void) {
    signed char arr[5] = {-128, -120, -2, 1, 120};
    unsigned char u_arr[4] = {0, 170, 250, 255};

    arr[0] ^= 12345;
    arr[1] |= u_arr[3];
    arr[2] &= u_arr[1] - (unsigned char) 185;
    arr[3] <<= 7u;
    static long x = 32;
    arr[4] >>= 31;

    u_arr[3] <<= 12;
    u_arr[2] >>= (x - 1);
    u_arr[1] |= -399;
    x = -4296140120l;
    u_arr[0] ^= x;

    if (arr[0] != -71) return 1;
    if (arr[1] != -1) return 2;
    if (arr[2] != -16) return 3;
    if (arr[3] != -128) return 4;
    if (arr[4] != 0) return 5;
    if (u_arr[0] != 168) return 6;
    if (u_arr[1] != 251) return 7;
    if (u_arr[2] != 0) return 8;
    if (u_arr[3] != 0) return 9;

    return 0;
}
