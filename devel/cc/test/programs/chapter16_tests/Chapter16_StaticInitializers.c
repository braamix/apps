/* Test that initializers for static objects with character type are correctly
 * converted to the correct type */

char from_long = 1099511627520l;        // low byte 0
char from_double = 15.6;
char from_uint = 2147483777u;            // low byte 129
char from_ulong = 281474976710410ul;     // low byte 10
signed char sc_long = 1099511627523l; // low byte 3
signed char sc_uint = 2147483898u;
signed char sc_ulong = 281474976710410ul; // low byte 10
signed char sc_dbl = 1e-10;
unsigned char uc_int = 13526;
unsigned char uc_uint = 2147483898u;
unsigned char uc_long = 1099511627770l;    // low byte 250
unsigned char uc_ulong = 281474976710410ul; // low byte 10
unsigned char uc_dbl = 77.7;

int main(void) {
    if (from_long != 0) return 1;
    if (from_double != 15) return 2;
    if (from_uint != 129) return 3;  // plain char unsigned on BESM-6
    if (from_ulong != 10) return 4;
    if (sc_uint != -6) return 5;
    if (sc_ulong != 10) return 6;
    if (sc_dbl != 0) return 7;
    if (uc_int != 214) return 8;
    if (uc_uint != 250) return 9;
    if (uc_ulong != 10) return 10;
    if (uc_dbl != 77) return 11;
    if (sc_long != 3) return 12;
    if (uc_long != 250) return 13;
    return 0;
}
