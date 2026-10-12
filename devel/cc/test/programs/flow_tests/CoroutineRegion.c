        int main(void)
        {
            char src[20] = "abcdefghijklmnopqrs", dst[20] = { 0 };
            duff(dst, src, 13);
            if (dst[12] != 'm' || dst[13] != 0 || dst[0] != 'a')
                return 1;
            if (f(0) != 104 || f(1) != 101)
                return 2;
            return 0;
        }

