/* Pass structs of sizes 13..24 bytes by value, validated byte-exact with memcmp. Split from the book's StructSizes (see StructSizes1). */

int memcmp(void *s1, void *s2, unsigned long n);

struct bytesize13 { unsigned char arr[13]; };
extern struct bytesize13 gvar13;
struct bytesize14 { unsigned char arr[14]; };
extern struct bytesize14 gvar14;
struct bytesize15 { unsigned char arr[15]; };
extern struct bytesize15 gvar15;
struct bytesize16 { unsigned char arr[16]; };
extern struct bytesize16 gvar16;
struct bytesize17 { unsigned char arr[17]; };
extern struct bytesize17 gvar17;
struct bytesize18 { unsigned char arr[18]; };
extern struct bytesize18 gvar18;
struct bytesize19 { unsigned char arr[19]; };
extern struct bytesize19 gvar19;
struct bytesize20 { unsigned char arr[20]; };
extern struct bytesize20 gvar20;
struct bytesize21 { unsigned char arr[21]; };
extern struct bytesize21 gvar21;
struct bytesize22 { unsigned char arr[22]; };
extern struct bytesize22 gvar22;
struct bytesize23 { unsigned char arr[23]; };
extern struct bytesize23 gvar23;
struct bytesize24 { unsigned char arr[24]; };
extern struct bytesize24 gvar24;

int chk0(struct bytesize13 s13, struct bytesize14 s14, struct bytesize15 s15, struct bytesize16 s16, struct bytesize17 s17, struct bytesize18 s18, unsigned char *e13, unsigned char *e14, unsigned char *e15, unsigned char *e16, unsigned char *e17, unsigned char *e18);
int chk1(struct bytesize19 s19, struct bytesize20 s20, struct bytesize21 s21, struct bytesize22 s22, struct bytesize23 s23, struct bytesize24 s24, unsigned char *e19, unsigned char *e20, unsigned char *e21, unsigned char *e22, unsigned char *e23, unsigned char *e24);

int main(void) {
    if (!chk0(gvar13, gvar14, gvar15, gvar16, gvar17, gvar18, gvar13.arr, gvar14.arr, gvar15.arr, gvar16.arr, gvar17.arr, gvar18.arr)) return 1;
    if (!chk1(gvar19, gvar20, gvar21, gvar22, gvar23, gvar24, gvar19.arr, gvar20.arr, gvar21.arr, gvar22.arr, gvar23.arr, gvar24.arr)) return 2;

    struct bytesize13 loc13 = {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}};
    struct bytesize14 loc14 = {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14}};
    struct bytesize15 loc15 = {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}};
    struct bytesize16 loc16 = {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}};
    struct bytesize17 loc17 = {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17}};
    struct bytesize18 loc18 = {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18}};
    struct bytesize19 loc19 = {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19}};
    struct bytesize20 loc20 = {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20}};
    struct bytesize21 loc21 = {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21}};
    struct bytesize22 loc22 = {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22}};
    struct bytesize23 loc23 = {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23}};
    struct bytesize24 loc24 = {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24}};

    if (!chk0(loc13, loc14, loc15, loc16, loc17, loc18, loc13.arr, loc14.arr, loc15.arr, loc16.arr, loc17.arr, loc18.arr)) return 3;
    if (!chk1(loc19, loc20, loc21, loc22, loc23, loc24, loc19.arr, loc20.arr, loc21.arr, loc22.arr, loc23.arr, loc24.arr)) return 4;
    return 0;
}

struct bytesize13 gvar13 = {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}};
struct bytesize14 gvar14 = {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14}};
struct bytesize15 gvar15 = {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}};
struct bytesize16 gvar16 = {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}};
struct bytesize17 gvar17 = {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17}};
struct bytesize18 gvar18 = {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18}};
struct bytesize19 gvar19 = {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19}};
struct bytesize20 gvar20 = {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20}};
struct bytesize21 gvar21 = {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21}};
struct bytesize22 gvar22 = {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22}};
struct bytesize23 gvar23 = {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23}};
struct bytesize24 gvar24 = {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24}};

int chk0(struct bytesize13 s13, struct bytesize14 s14, struct bytesize15 s15, struct bytesize16 s16, struct bytesize17 s17, struct bytesize18 s18, unsigned char *e13, unsigned char *e14, unsigned char *e15, unsigned char *e16, unsigned char *e17, unsigned char *e18) {
    if (memcmp(&s13, e13, sizeof s13)) return 0;
    if (memcmp(&s14, e14, sizeof s14)) return 0;
    if (memcmp(&s15, e15, sizeof s15)) return 0;
    if (memcmp(&s16, e16, sizeof s16)) return 0;
    if (memcmp(&s17, e17, sizeof s17)) return 0;
    if (memcmp(&s18, e18, sizeof s18)) return 0;
    return 1;
}
int chk1(struct bytesize19 s19, struct bytesize20 s20, struct bytesize21 s21, struct bytesize22 s22, struct bytesize23 s23, struct bytesize24 s24, unsigned char *e19, unsigned char *e20, unsigned char *e21, unsigned char *e22, unsigned char *e23, unsigned char *e24) {
    if (memcmp(&s19, e19, sizeof s19)) return 0;
    if (memcmp(&s20, e20, sizeof s20)) return 0;
    if (memcmp(&s21, e21, sizeof s21)) return 0;
    if (memcmp(&s22, e22, sizeof s22)) return 0;
    if (memcmp(&s23, e23, sizeof s23)) return 0;
    if (memcmp(&s24, e24, sizeof s24)) return 0;
    return 1;
}

