/* The Apple wchar_t conversion extension is not ported — there is no wchar_t
 * runtime here — but the headers still name the type. */
#ifndef _WCHAR_H_
#define _WCHAR_H_

#include <sys/types.h>

typedef int wchar_t_dummy_;

#endif
