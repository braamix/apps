union u { int a; };
union u;
int main(void) {
    union u my_union = {1};
    return my_union.a;
}
