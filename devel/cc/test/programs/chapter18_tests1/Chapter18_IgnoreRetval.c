struct small { int x; };
struct big { double d; int x; long l; };
struct small globl = {0};
struct small ret_reg(void) { globl.x = globl.x + 1; return globl; }
struct big globl2 = {1.25, 2, 300};
struct big ret_mem(void) {
    globl2.d = globl2.d * 2;
    globl2.x = globl2.x * 3;
    globl2.l = globl2.l * 4;
    return globl2;
}
int main(void) {
    (void)ret_reg();
    ret_reg();
    if (globl.x != 2) return 1;
    ret_mem();
    (void)ret_mem();
    if (globl2.d != 5.0 || globl2.x != 18 || globl2.l != 4800) return 2;
    return 0;
}
