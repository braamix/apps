int main(void) {
    int *null_ptr = 0;
    if (&*null_ptr != 0)
        return 1;

    int **ptr_to_null = &null_ptr;

    if (&**ptr_to_null)
        return 2;

    return 0;
}
