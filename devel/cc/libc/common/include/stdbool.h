/*
 * <stdbool.h> — boolean type and values (C11 §7.18).
 *
 * Converting any scalar to _Bool is a zero test, so a _Bool object never holds
 * anything but 0 or 1.
 */
#ifndef _STDBOOL_H
#define _STDBOOL_H

#define bool  _Bool
#define true  1
#define false 0

#define __bool_true_false_are_defined 1

#endif /* _STDBOOL_H */
