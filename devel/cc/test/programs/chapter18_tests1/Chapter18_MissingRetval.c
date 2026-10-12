struct big { char arr[25]; };
struct big missing_return_value(int *i);
int main(void) {
    int array[4] = {1, 2, 3, 4};
    missing_return_value(array + 2);
    return array[2] == 10;
}
struct big missing_return_value(int *i) {
    *i = 10;
    struct big result;
    return result;
}

