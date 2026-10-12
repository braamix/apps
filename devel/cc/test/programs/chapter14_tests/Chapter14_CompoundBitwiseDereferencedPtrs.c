unsigned long ul = 18446460386757245432ul;

int main(void) {

    unsigned long *ul_ptr = &ul;
    *ul_ptr &= -1000;
    if (ul != 18446460386757244952ul ) {
        return 1;
    }
    *ul_ptr |= 4294967040u;

    if (ul != 18446460386824683288ul ) {
        return 2;
    }
    int i = 123456;
    unsigned int ui = 4042322160u; // 2^32 - 252645136
    long l = -252645136;
    unsigned int *ui_ptr = &ui;
    long *l_ptr = &l;
    if (*ui_ptr ^= *l_ptr) {
        return 3;
    }
    if (ui) {
        return 4;
    }

    if (i != 123456) {
        return 5;
    }
    if (l != -252645136) {
        return 6;
    }

    return 0;
}
