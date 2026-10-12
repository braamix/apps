        #include <stdlib.h>
        #include <string.h>
        int main(void)
        {
            char *small = malloc(10);
            int *big = calloc(1000000, sizeof(int)); // 4 MB: memory.grow
            if (!small || !big) return 1;
            if (((unsigned long)small & 15) || ((unsigned long)big & 15)) return 2;
            for (int i = 0; i < 1000000; i += 1000)
                if (big[i]) return 3;
            big[999999] = 42;
            strcpy(small, "abc");
            char *more = realloc(small, 100);
            if (strcmp(more, "abc") != 0) return 4;
            return big[999999];
        }
    
