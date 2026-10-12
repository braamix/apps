//
// Closing a stream from open_memstream() in a plain function.
//
#include <stdio.h>
#include <stdlib.h>

#include "host.h"

int memstream_close(FILE *f)
{
#ifdef __braam__
    // fclose is a coroutine here, but one on a memory stream never waits: run it to
    // its end in a frame of its own, with room for the fflush it awaits.
    co_frame(braam_call *, int) *run = co_alloca(fclose, 4096, f);
    if (co_resume(run) != CO_DONE)
        abort();
    return co_result(run);
#else
    return fclose(f);
#endif
}
