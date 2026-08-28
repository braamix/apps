#include "estruct.h"
#include "version.h"

#include "proc/io.h"

Task<void> version(void)
{
    co_await write_all(SYS_STDOUT, PROGRAM_NAME_LONG " version " VERSION "\n");
}
