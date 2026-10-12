// Test compound assignment with characters; make sure we perform integer promotions

int main(void) {

    static signed char c = 100; // plain char unsigned on BESM-6; keep signed wrap
    signed char c2 = 100;
    c += c2; // well-defined b/c of integer promotions
    if (c != -56) {
        return 1; // fail
    }

    static unsigned char uc = 200;
    c2 = -100;
    uc /= c2; // convert uc and c2 to int, then convert back
    if (uc != 254) {
        return 2; // fail
    }

    uc -= 250.0; // convert uc to double, do operation, convert back
    if (uc != 4) {
         return 3;  // fail
    }

    static signed char sc = 70;
    sc = -sc;
    sc *= c;
    if (sc != 80) {
        return 4; // fail
    }

    if ((sc %= c) != 24) {
        return 5;
    }

    return 0;
}
