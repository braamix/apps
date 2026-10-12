        int sum(int n)
        {
            int a[100];
            for (int i = 0; i < 100; i++)
                a[i] = n;
            if (n == 0)
                return 0;
            int s = sum(n - 1);
            return s + a[99];
        }
        int depth(int *p, int n) { int x = n; if (n) return depth(&x, n - 1) + *p; return *p; }
        int main(void)
        {
            int base = 0;
            if (sum(200) != 20100) return 1;
            if (depth(&base, 10) != 55) return 2;
            return 7;
        }
    
