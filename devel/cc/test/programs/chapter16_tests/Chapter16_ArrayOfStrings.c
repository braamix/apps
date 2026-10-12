/* Test declaring and operating on an array of pointers to strings */

int strcmp(char *s1, char *s2) {
    while (*s1 && *s1 == *s2) {
        s1 = s1 + 1;
        s2 = s2 + 1;
    }
    return *s1 - *s2;
}

int main(void) {
    char *strings[4] = {"yes", "no", "maybe"};
    if (strcmp(strings[0], "yes")) {
        return 1;
    }
    if (strcmp(strings[1], "no")) {
        return 2;
    }
    if (strcmp(strings[2], "maybe")) {
        return 3;
    }
    if (strings[3]) {
        return 4;
    }

    return 0;
}
