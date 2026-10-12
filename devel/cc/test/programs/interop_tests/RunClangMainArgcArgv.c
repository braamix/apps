        #include <stdio.h>
        coro(braam_call *) int main(int argc, char **argv)
        {
            await putchar('o'); await putchar('k'); await putchar('\n');
            return argc + (argv != 0) + 4;
        }
    
