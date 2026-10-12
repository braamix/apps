#include <alloca.h>
#include <braam.h>
coro(braam_call *) int main(int argc, char **argv)
{
    char *p = alloca(16);
    p[0]    = 0;
    return p[0];
}

