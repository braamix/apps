/* Make sure the parser concatenates adjacent string literals */

int strcmp(char *s1, char *s2);  // from standard library

int main(void) {
    char multi_string[6] =
        "YES"
        "NO";  // can concatenate two string literals in an initializer
    char nested_multi_string[2][3] = {
        "A"
        "B",
        "C"
        "D"};  // first element is "AB", second is "CD"

    // validate multi_string
    if (strcmp(multi_string, "YESNO"))
        return 1;
    if (strcmp(nested_multi_string[0], "AB"))
        return 2;
    if (strcmp(nested_multi_string[1], "CD"))
        return 3;
    return 0;
}
