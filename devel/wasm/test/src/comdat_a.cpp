// Both objects hold comdat_counter and comdat_scaled<3>; they must share them.
#include "comdat.h"
#include "fixture.h"

extern "C" void fx_main(void)
{
    *comdat_counter() += 1;
    comdat_b_bump();
    fx_puts(comdat_counter() == comdat_b_counter() ? "shared\n" : "not shared\n");
    fx_puts("count ");
    fx_putn(*comdat_counter());
    fx_puts("\nscaled ");
    fx_putn(comdat_scaled<3>(10));
    fx_puts("\n");
}
