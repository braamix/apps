#ifndef _SYS_PARAM_H_
#define _SYS_PARAM_H_

#include <sys/types.h>

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#define powerof2(x) ((((x) - 1) & (x)) == 0)

#endif
