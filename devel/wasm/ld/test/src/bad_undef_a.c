// Strong references nothing defines: an error naming each referencing file.
#include "fixture.h"

extern int nowhere(void);
extern int nowhere_var;
extern int nowhere_b(void);

void fx_main(void)
{
    fx_putn(nowhere() + nowhere() + nowhere_var + nowhere_b());
}
