int main(void) {
    int a = 10;
    switch(a) {
        case 1: return 1;
        extern int x;
        case 2: return 2;
        case 10:
        if (x * 2 == 30) {
            return 0;
        }
        default: return 5;
    }
    return 6;
}

int x = 15;
