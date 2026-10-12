// Another test that we promote switch controlling condition to integer type
int main(void) {
    char c = -56;
    switch (c) {
        // if we reduced this to a char it would be -56
        // but we won't, so this case shouldn't be taken
        case 33554632:
            return 1;  // fail
        default:
            return 0;
    }
}
