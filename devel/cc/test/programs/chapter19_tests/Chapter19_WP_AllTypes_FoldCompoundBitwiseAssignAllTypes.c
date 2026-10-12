/* Test copy prop/constant folding of compound bitwise assignment with non-int
 * types and type conversions.  Adapted for BESM-6: int/long are 41-bit and
 * unsigned is 48-bit, so the book's 64-bit bit patterns are replaced with
 * in-range values; right shift is logical on BESM-6.  The char cases (which
 * exercise integer promotion, including shift counts wider than a char) are kept
 * unchanged.  Function names are kept distinct within 8 characters.
 */

// like Chapter 16's compound_bitwise_ops_chars.c, constant-foldable
int t_chars(void) {
    signed char c1 = -128;
    signed char c2 = -120;
    signed char c3 = -2;
    signed char c4 = 1;
    signed char c5 = 120;

    unsigned char u1 = 0;
    unsigned char u2 = 170;
    unsigned char u3 = 250;
    unsigned char u4 = 255;

    // bitwise ops on signed chars (well-defined via integer promotion)
    c1 ^= 12345;
    c2 |= u4;
    c3 &= u2 - (unsigned char)185;
    c4 <<= 7u;   // wraps to -128 after promotion
    c5 >>= 31;   // shift count wider than char is fine after promotion

    // bitwise ops on unsigned chars
    long x = 32;
    u4 <<= 12;
    u3 >>= (x - 1);
    u2 |= -399;
    x = -4296140120l;  // fits in a 41-bit long
    u1 ^= x;

    if (c1 != -71) {
        return 1; // fail
    }
    if (c2 != -1) {
        return 2; // fail
    }
    if (c3 != -16) {
        return 3; // fail
    }
    if (c4 != -128) {
        return 4; // fail
    }
    if (c5) {
        return 5; // fail
    }
    if (u1 != 168) {
        return 6; // fail
    }
    if (u2 != 251) {
        return 7; // fail
    }
    if (u3) {
        return 8; // fail
    }
    if (u4) {
        return 9; // fail
    }
    return 0;
}

// long bitwise &/|/^ with in-range (41-bit) patterns
int t_lbw(void) {
    long l1 = 1095216660735l;  // 0xFF00FF00FF
    long l2 = -4294967296l;    // -2^32
    l1 &= l2;
    if (l1 != 1095216660480l) {
        return 1; // fail
    }
    l2 |= 100l;
    if (l2 != -4294967196l) {
        return 2; // fail
    }
    l1 = 1095216660735l;
    l1 ^= 824633720833l;  // 0xC000000001
    if (l1 != 270582939902l) {
        return 3; // fail
    }
    return 0;
}

// shifts on int and long with long/int shift counts (results stay in 41 bits)
int t_lsh(void) {
    int x = 100;
    x <<= 22l;
    if (x != 419430400) {
        return 1; // fail
    }
    if ((x >>= 4l) != 26214400) {
        return 2; // fail
    }
    if (x != 26214400) {
        return 3; // fail
    }
    long l = 12345l;
    l <<= 20;
    if (l != 12944670720l) {
        return 4; // fail
    }
    return 0;
}

// unsigned bitwise &/|/^ with in-range (48-bit) values
int t_ubw(void) {
    unsigned long ul = 281474976710655UL;  // 2^48 - 1
    ul &= 4294967295u;  // zero-extend to 48 bits
    if (ul != 4294967295ul) {
        return 1; // fail
    }
    ul = 1095216660735UL;  // 0xFF00FF00FF
    ul |= 4294967040u;     // 0xFFFFFF00
    if (ul != 1099511627775ul) {
        return 2; // fail
    }
    unsigned int ui = 4042322160u;  // 0xF0F0F0F0
    ui ^= 252645135u;               // 0x0F0F0F0F
    if (ui != 4294967295u) {
        return 3; // fail
    }
    return 0;
}

// unsigned shifts (right shift is logical, zero-fill)
int t_ush(void) {
    unsigned long ul = 281474976710655UL;  // 2^48 - 1
    ul >>= 20;
    if (ul != 268435455ul) {
        return 1; // fail
    }
    unsigned int u = 255u;
    u <<= 12;
    if (u != 1044480u) {
        return 2; // fail
    }
    return 0;
}

int main(void) {
    if (t_chars()) {
        return 1; // fail
    }
    if (t_lbw()) {
        return 2; // fail
    }
    if (t_lsh()) {
        return 3; // fail
    }
    if (t_ubw()) {
        return 4; // fail
    }
    if (t_ush()) {
        return 5; // fail
    }
    return 0; // success
}

