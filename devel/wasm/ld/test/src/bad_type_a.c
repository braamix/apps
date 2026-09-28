// One name defined as data here and as a function in bad_type_b.c: an error.
#include "fixture.h"

int clash = 3;

void fx_main(void)
{
    fx_putn(clash);
}
