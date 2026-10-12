void *memset(void *s, int c, unsigned long n);

static char zeroed_bytes[100];  // zero-initialized static storage (replaces calloc)

void *get_100_zeroed_bytes(void) {
    return zeroed_bytes;
}

void fill_100_bytes(void *pointer, int byte) {
    memset(pointer, byte, 100);
}

int main(void) {

    void *mem = get_100_zeroed_bytes();
    for (int i = 0; i < 100; i = i + 1) {
        if (((char *) mem + i)[0]) {
            return 1;
        }
    }

    fill_100_bytes(mem, 99);

    for (int i = 0; i < 100; i = i + 1) {
        if (((char *) mem + i)[0] != 99) {
            return 2;
        }
    }

    return 0;
}
