/* Pass structs of sizes 1..12 bytes by value, validated byte-exact with memcmp. Split from the book's StructSizes (passing all sizes 1..24 through one function exceeds the BESM-6 address range). */

int memcmp(void *s1, void *s2, unsigned long n);

struct bytesize1 { unsigned char arr[1]; };
extern struct bytesize1 gvar1;
struct bytesize2 { unsigned char arr[2]; };
extern struct bytesize2 gvar2;
struct bytesize3 { unsigned char arr[3]; };
extern struct bytesize3 gvar3;
struct bytesize4 { unsigned char arr[4]; };
extern struct bytesize4 gvar4;
struct bytesize5 { unsigned char arr[5]; };
extern struct bytesize5 gvar5;
struct bytesize6 { unsigned char arr[6]; };
extern struct bytesize6 gvar6;
struct bytesize7 { unsigned char arr[7]; };
extern struct bytesize7 gvar7;
struct bytesize8 { unsigned char arr[8]; };
extern struct bytesize8 gvar8;
struct bytesize9 { unsigned char arr[9]; };
extern struct bytesize9 gvar9;
struct bytesize10 { unsigned char arr[10]; };
extern struct bytesize10 gvar10;
struct bytesize11 { unsigned char arr[11]; };
extern struct bytesize11 gvar11;
struct bytesize12 { unsigned char arr[12]; };
extern struct bytesize12 gvar12;

int chk0(struct bytesize1 s1, struct bytesize2 s2, struct bytesize3 s3, struct bytesize4 s4, struct bytesize5 s5, struct bytesize6 s6, unsigned char *e1, unsigned char *e2, unsigned char *e3, unsigned char *e4, unsigned char *e5, unsigned char *e6);
int chk1(struct bytesize7 s7, struct bytesize8 s8, struct bytesize9 s9, struct bytesize10 s10, struct bytesize11 s11, struct bytesize12 s12, unsigned char *e7, unsigned char *e8, unsigned char *e9, unsigned char *e10, unsigned char *e11, unsigned char *e12);

int main(void) {
    if (!chk0(gvar1, gvar2, gvar3, gvar4, gvar5, gvar6, gvar1.arr, gvar2.arr, gvar3.arr, gvar4.arr, gvar5.arr, gvar6.arr)) return 1;
    if (!chk1(gvar7, gvar8, gvar9, gvar10, gvar11, gvar12, gvar7.arr, gvar8.arr, gvar9.arr, gvar10.arr, gvar11.arr, gvar12.arr)) return 2;

    struct bytesize1 loc1 = {{1}};
    struct bytesize2 loc2 = {{1, 2}};
    struct bytesize3 loc3 = {{1, 2, 3}};
    struct bytesize4 loc4 = {{1, 2, 3, 4}};
    struct bytesize5 loc5 = {{1, 2, 3, 4, 5}};
    struct bytesize6 loc6 = {{1, 2, 3, 4, 5, 6}};
    struct bytesize7 loc7 = {{1, 2, 3, 4, 5, 6, 7}};
    struct bytesize8 loc8 = {{1, 2, 3, 4, 5, 6, 7, 8}};
    struct bytesize9 loc9 = {{1, 2, 3, 4, 5, 6, 7, 8, 9}};
    struct bytesize10 loc10 = {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10}};
    struct bytesize11 loc11 = {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}};
    struct bytesize12 loc12 = {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}};

    if (!chk0(loc1, loc2, loc3, loc4, loc5, loc6, loc1.arr, loc2.arr, loc3.arr, loc4.arr, loc5.arr, loc6.arr)) return 3;
    if (!chk1(loc7, loc8, loc9, loc10, loc11, loc12, loc7.arr, loc8.arr, loc9.arr, loc10.arr, loc11.arr, loc12.arr)) return 4;
    return 0;
}

struct bytesize1 gvar1 = {{1}};
struct bytesize2 gvar2 = {{1, 2}};
struct bytesize3 gvar3 = {{1, 2, 3}};
struct bytesize4 gvar4 = {{1, 2, 3, 4}};
struct bytesize5 gvar5 = {{1, 2, 3, 4, 5}};
struct bytesize6 gvar6 = {{1, 2, 3, 4, 5, 6}};
struct bytesize7 gvar7 = {{1, 2, 3, 4, 5, 6, 7}};
struct bytesize8 gvar8 = {{1, 2, 3, 4, 5, 6, 7, 8}};
struct bytesize9 gvar9 = {{1, 2, 3, 4, 5, 6, 7, 8, 9}};
struct bytesize10 gvar10 = {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10}};
struct bytesize11 gvar11 = {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}};
struct bytesize12 gvar12 = {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}};

int chk0(struct bytesize1 s1, struct bytesize2 s2, struct bytesize3 s3, struct bytesize4 s4, struct bytesize5 s5, struct bytesize6 s6, unsigned char *e1, unsigned char *e2, unsigned char *e3, unsigned char *e4, unsigned char *e5, unsigned char *e6) {
    if (memcmp(&s1, e1, sizeof s1)) return 0;
    if (memcmp(&s2, e2, sizeof s2)) return 0;
    if (memcmp(&s3, e3, sizeof s3)) return 0;
    if (memcmp(&s4, e4, sizeof s4)) return 0;
    if (memcmp(&s5, e5, sizeof s5)) return 0;
    if (memcmp(&s6, e6, sizeof s6)) return 0;
    return 1;
}
int chk1(struct bytesize7 s7, struct bytesize8 s8, struct bytesize9 s9, struct bytesize10 s10, struct bytesize11 s11, struct bytesize12 s12, unsigned char *e7, unsigned char *e8, unsigned char *e9, unsigned char *e10, unsigned char *e11, unsigned char *e12) {
    if (memcmp(&s7, e7, sizeof s7)) return 0;
    if (memcmp(&s8, e8, sizeof s8)) return 0;
    if (memcmp(&s9, e9, sizeof s9)) return 0;
    if (memcmp(&s10, e10, sizeof s10)) return 0;
    if (memcmp(&s11, e11, sizeof s11)) return 0;
    if (memcmp(&s12, e12, sizeof s12)) return 0;
    return 1;
}

