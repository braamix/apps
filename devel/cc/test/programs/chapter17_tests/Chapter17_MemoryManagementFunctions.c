/* Test that we can write, grow, and read back statically allocated buffers */

int main(void) {
    static char char_buffer[100];  // already the post-"realloc" size
    for (int i = 0; i < 50; i = i + 1) {
        char_buffer[i] = i;
    }

    char *char_buffer2 = char_buffer;  // "realloc" reuses the same storage
    char_buffer2[75] = 11;

    for (int i = 0; i < 50; i = i + 1) {
        if ( char_buffer2[i] != i) {
            return 1;
        }
    }

    if (char_buffer2[75] != 11) {
        return 2;
    }

    static double double_buffer[10];  // zero-initialized (replaces calloc)
    for (int i = 0; i < 10; i = i + 1) {
        if (double_buffer[i]) {
            return 3;
        }
    }
    return 0;
}
