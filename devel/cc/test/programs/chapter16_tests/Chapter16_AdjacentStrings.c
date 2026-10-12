/* Test that we concatenate adjacent string literal tokens */

int puts(char *s);

int main(void) {
    char *strings = "HELLO," " WORLD";
    puts(strings);
    return 0;
}
