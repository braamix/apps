// The second copy of comdat.h's functions.
#include "comdat.h"

int comdat_b_bump()
{
    *comdat_counter() += 10;
    return comdat_scaled<3>(1);
}

int *comdat_b_counter()
{
    return comdat_counter();
}
