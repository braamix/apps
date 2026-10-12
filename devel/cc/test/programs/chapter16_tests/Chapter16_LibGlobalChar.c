extern char c;
extern unsigned char uc;
extern signed char sc;

int update_global_chars(void);

int main(void) {
    if (c != 100) {
        return 1;
    }

    if (uc != 250) {
        return 2;
    }

    if (sc != 0) {
        return 3;
    }

    update_global_chars();

    if (c != 110) {
        return 4;
    }

    if (uc != 4) {
        return 5;
    }

    if (sc != -10) {
        return 6;
    }

    return 0;
}

char c = 100;
unsigned char uc = 250;
signed char sc = 0;

int update_global_chars(void) {
    c = c + 10;
    uc = uc + 10; // wraps around
    sc = sc - 10;
    return 0;
}
