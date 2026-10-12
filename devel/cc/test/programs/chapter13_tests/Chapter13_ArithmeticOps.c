double point_one = 0.25;
double point_two = 0.5;
double point_three = 0.125;
double two = 2.0;
double three = 3.0;
double four = 4.0;
double twelveE15 = 12e15;

int addition(void) {
    return (point_one + point_two == 0.75);
}
int subtraction(void) {
    return (four - 1.0 == 3.0);
}
int multiplication(void) {
    return (point_two * point_three == 0.0625);
}
int division(void) {
    return (7.0 / two == 3.5);
}
int negation(void) {
    double neg = -twelveE15;
    return !(12e15 + neg);
}
int complex_expression(void) {
    double result = (two + three) - 127.5 * four;
    return result == -505.0;
}

int main(void) {
    if (!addition()) { return 1; }
    if (!subtraction()){ return 2; }
    if (!multiplication()) { return 3; }
    if (!division()) { return 4; }
    if (!negation()) { return 5; }
    if (!complex_expression()) { return 5; }
    return 0;
}
