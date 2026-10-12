/* When we initialize an array from a string literal,
 * make sure we include the null byte if the array has space for it,
 * and exclude it otherwise
 * */

int strcmp(char *s1, char *s2);  // standard library

int test_flat_static_with_null_byte(void) {
    static unsigned char flat[4] = "DOG";
    return (flat[0] == 'D' && flat[1] == 'O' && flat[2] == 'G' && flat[3] == 0);
}

int test_nested_static_with_null_byte(void) {
    // Renamed from `nested` to avoid shadowing the file-scope `nested` below
    // (no-shadowing is a permanent design decision); behaviour is unchanged.
    static char nested_s[2][4] = {"YES", "YUP"};
    return (nested_s[0][0] == 'Y' && nested_s[0][1] == 'E' && nested_s[0][2] == 'S' &&
            nested_s[0][3] == 0 && nested_s[1][0] == 'Y' && nested_s[1][1] == 'U' &&
            nested_s[1][2] == 'P' && nested_s[1][3] == 0);
}

int test_flat_auto_with_null_byte(void) {
    char flat_auto[2] = "X";
    return (flat_auto[0] == 'X' && flat_auto[1] == 0);
}

int test_nested_auto_with_null_byte(void) {
    char nested_auto[2][2][2] = {{"A", "B"}, {"C", "D"}};
    return (nested_auto[0][0][0] == 'A' && nested_auto[0][0][1] == 0 &&
            nested_auto[0][1][0] == 'B' && nested_auto[0][1][1] == 0 &&
            nested_auto[1][0][0] == 'C' && nested_auto[1][0][1] == 0 &&
            nested_auto[1][1][0] == 'D' && nested_auto[1][1][1] == 0);
}

int test_flat_static_without_null_byte(void) {
    static char letters[4] = "ABCD";
    return letters[0] == 'A' && letters[1] == 'B' && letters[2] == 'C' &&
           letters[3] == 'D';
}

char nested[3][3] = {"YES", "NO", "OK"};
int test_nested_static_without_null_byte(void) {
    char *whole_array = (char *)nested;
    char *word1 = (char *)nested[0];
    char *word2 = (char *)nested[1];
    char *word3 = (char *)nested[2];
    return !(strcmp(whole_array, "YESNO") || strcmp(word1, "YESNO") ||
             strcmp(word2, "NO") || strcmp(word3, "OK"));
}

int test_flat_auto_without_null_byte(void) {
    int x = -1;
    char letters[4] = "ABCD";
    int y = -1;
    return (x == -1 && y == -1 && letters[0] == 'A' && letters[1] == 'B' &&
            letters[2] == 'C' && letters[3] == 'D');
}

int test_nested_auto_without_null_byte(void) {
    // Renamed from `nested` to avoid shadowing the file-scope `nested` (no-shadowing
    // is a permanent design decision); behaviour is unchanged.
    char nested_a[3][3] = {"YES", "NO", "OK"};
    char *whole_array = (char *)nested_a;
    char *word1 = (char *)nested_a[0];
    char *word2 = (char *)nested_a[1];
    char *word3 = (char *)nested_a[2];
    return !(strcmp(whole_array, "YESNO") || strcmp(word1, "YESNO") ||
             strcmp(word2, "NO") || strcmp(word3, "OK"));
}

int main(void) {
    if (!test_flat_static_with_null_byte()) return 1;
    if (!test_nested_static_with_null_byte()) return 2;
    if (!test_flat_auto_with_null_byte()) return 3;
    if (!test_nested_auto_with_null_byte()) return 4;
    if (!test_flat_static_without_null_byte()) return 5;
    if (!test_nested_static_without_null_byte()) return 6;
    if (!test_flat_auto_without_null_byte()) return 7;
    if (!test_nested_auto_without_null_byte()) return 8;
    return 0;
}
