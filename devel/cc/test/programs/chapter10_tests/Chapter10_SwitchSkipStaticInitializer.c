int a = 3;
int main(void) {
    switch (a) {
        case 1:;
            static int x = 10;
            x = 0;
        case 3:
            return x;
    }
    return 0;
}
