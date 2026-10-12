        int fib(int n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }
        int main(void)
        {
            int count = 0;
            for (int i = 0; i < 10; i++)
                for (int j = 0; j < i; j++)
                    if ((i + j) % 3 == 0)
                        count++;
            return fib(10) + count; // 55 + 15
        }
    
