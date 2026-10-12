        long long g[5] = { 1, 2, 3, 4, 5 };
        int m[3][4];
        int main(void)
        {
            long long *p = g + 4, *q = &g[1];
            if (p - q != 3) return 1;
            if (*--p != 4 || p[-2] != 2) return 2;
            for (int i = 0; i < 3; i++)
                for (int j = 0; j < 4; j++)
                    m[i][j] = i * 10 + j;
            int (*row)[4] = m + 2;
            if ((*row)[3] != 23 || m[1][2] != 12) return 3;
            int x = 5, *px = &x, **ppx = &px;
            **ppx += 1;
            if (x != 6) return 4;
            char buf[8];
            char *e = buf + sizeof buf;
            if (e - buf != 8) return 5;
            return 0;
        }
    
