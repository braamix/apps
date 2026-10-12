int main(void) {

    unsigned long ul = 18446460386757245432ul;
    // -1000 sign-extends to unsigned long.
    ul &= -1000;
    if (ul != 18446460386757244952ul ) {
        return 1; // fail
    }

    ul |= 4294967040u; // 0xffff_ff00 - zero-extended to unsigned long

    if (ul != 18446460386824683288ul ) {
        return 2; // fail
    }

    int i = 123456;

    unsigned int ui = 4042322160u;
    long l = -252645136;
    if (ui ^= l) {
        return 3; // fail
    }

    if (ui) {
        return 4; // fail
    }
    if (i != 123456) {
        return 5;
    }
    if (l != -252645136) {
        return 6;
    }

    return 0; // success
}
