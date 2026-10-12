        #include <stdio.h>
        #include <string.h>
        #include <stdlib.h>
        coro(braam_call *) int main(void)
        {
            char buf[32];
            strcpy(buf, "hello");
            strcat(buf, ", world");
            if (strlen(buf) != 12) return 1;
            if (strcmp(buf, "hello, world") != 0 || strcmp("a", "b") >= 0) return 2;
            if (strchr(buf, ',') != buf + 5 || strstr(buf, "wor") != buf + 7) return 3;
            memset(buf + 20, 'x', 4);
            if (memcmp(buf + 20, "xxxx", 4) != 0) return 4;
            memmove(buf + 1, buf, 5); // overlapping: "hhello world"
            if (memcmp(buf, "hhello world", 13) != 0) return 5;
            memcpy(buf, "hello", 5); // "helloo world"
            if (atoi("-1234") != -1234) return 6;
            await puts(buf);
            return 0;
        }
    
