#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
coro(braam_call *) int main(void) {
    await printf("%lld %llu %llx %jd %hhd %hd %zu %td\n", (long long)(-9223372036854775807LL - 1),
           18446744073709551615ULL, 0x123456789abcdefULL, (intmax_t)-5, 300, 70000,
           (size_t)7, (ptrdiff_t)-3);
    return 0;
}

