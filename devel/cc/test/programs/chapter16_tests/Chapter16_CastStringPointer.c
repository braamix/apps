/* Test casts from char * to other character pointer types */

int main(void) {
    char *c = "THIS IS A STRING!";
    unsigned char *uc = (unsigned char *)c;
    if (uc[3] != 'S') {
        return 1;
    }
    signed char *sc = (signed char *)c;
    if (sc[3] != 'S'){
            return 2;
        }
    return 0;
}
