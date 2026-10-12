#include <float.h>
int main(void)
{
    return ld_consts(0.1L, LDBL_MAX, LDBL_MIN, LDBL_TRUE_MIN, LDBL_EPSILON, 1.0L / 3,
                     -0.1L * 7, 123456789012345678901234567890.0L);
}

