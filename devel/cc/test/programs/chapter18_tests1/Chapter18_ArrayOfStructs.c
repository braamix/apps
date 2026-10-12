struct inner { long l; char arr[2]; };
struct outer { char a; struct inner b; };
int validate_struct_array(struct outer *struct_array);
static struct outer static_array[3] = {
    {0, {0, {0, 0}}}, {2, {3, {4, 5}}}, {4, {6, {8, 10}}}};
int main(void) {
    struct outer auto_array[3] = {
        {0, {0, {0, 0}}}, {2, {3, {4, 5}}}, {4, {6, {8, 10}}}};
    if (!validate_struct_array(static_array)) return 1;
    if (!validate_struct_array(auto_array)) return 2;
    return 0;
}
int validate_struct_array(struct outer *struct_array) {
    for (int i = 0; i < 3; i = i + 1) {
        if (struct_array[i].a != i * 2) return 0;
        if (struct_array[i].b.l != i * 3) return 0;
        if (struct_array[i].b.arr[0] != i * 4) return 0;
        if (struct_array[i].b.arr[1] != i * 5) return 0;
    }
    return 1;
}

