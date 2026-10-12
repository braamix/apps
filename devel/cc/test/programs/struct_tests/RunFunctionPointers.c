        struct Two { int a, b; };
        static int add(int a, int b) { return a + b; }
        static int sub(int a, int b) { return a - b; }
        static struct Two pair(int a) { struct Two t = { a, -a }; return t; }
        static int (*ops[])(int, int) = { add, sub };
        typedef int (*op)(int, int);
        static op pick(int k) { return k ? sub : add; }
        static int apply(op f, int a, int b) { return f(a, b); }
        int main(void)
        {
            struct Two (*pf)(int) = pair;
            if (ops[0](3, 4) != 7 || ops[1](3, 4) != -1)
                return 1;
            if (pick(1)(10, 3) != 7 || apply(pick(0), 10, 3) != 13)
                return 2;
            if (pf(5).b != -5)
                return 3;
            if (ops[0] != add || pick(1) != sub)
                return 4;
            return 0;
        }
    
