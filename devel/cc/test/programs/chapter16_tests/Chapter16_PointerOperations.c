/* Test standard pointer operations on string literals
 * including pointer arithmetic and subscripting.
 */

int main(void) {
    if ("ABCDEFG"[2] != 'C') {
        return 1;
    }

    char *ptr = "THIS IS A STRING!" + 10;  // point to "STRING!"
    if (*ptr != 'S') {
        return 2;
    }

    if (ptr[6] != '!') {
        return 3;
    }

    if (ptr[7]) {  // null byte
        return 4;
    }

    if (!"NOT A NULL POINTER!") {
        return 5;
    }
}
