/* Test constant folding of Not, JumpIfZero, and JumpIfNotZero with char
 * operands. (We don't test constant-folding of other operations on char because
 * they get promoted to int first.)
 * Function names kept distinct within 8 characters (Madlen truncates labels).
 * */
int putch(int c);

int t_notc(void) {
    char x = 256;  // 0
    return !x;     // 1
}

int t_notuc(void) {
    unsigned char x = 256;  // 0
    return !x;              // 1
}

int t_nottc(void) {
    char x = -1;
    return !x;  // 0;
}

int t_andsc(void) {
    signed char c = 0;
    return c && putch('a');  // return 0, eliminate call to putch
}

int t_andtc(void) {
    signed char c1 = 44;
    char c2 = c1 - 10;
    return c1 && c2;  // 1
}

int t_oruc(void) {
    unsigned char u = 250;
    return u || putch('a');  // return 1, eliminate call to putch
}

int t_orc(void) {
    char c = 250;
    return c || putch('a');  // return 1, eliminate call to putch
}

char t_brc(void) {
    unsigned char u = 250;
    u = u + 6;  // 0
    if (u) {    // eliminate this branch
        putch('a');
    }
    return u + 10;
}

int main(void) {
    if (t_notc() != 1) {
        return 1;  // fail
    }
    if (t_notuc() != 1) {
        return 2;  // fail
    }
    if (t_nottc() != 0) {
        return 3;  // fail
    }
    if (t_andsc() != 0) {
        return 4;  // fail
    }
    if (t_andtc() != 1) {
        return 5;  // fail
    }
    if (t_oruc() != 1) {
        return 6;  // fail
    }
    if (t_orc() != 1) {
        return 7;  // fail
    }
    if (t_brc() != 10) {
        return 8;  // fail
    }
    return 0;  // success
}

