/*
 * strerror for Braam: C's three errors and Braam's (errno.h).
 */
#include <string.h>

static const char *const braam[] = {
    "Invalid argument", "Out of memory",  "Not found",         "Exists",
    "Not a directory",  "Is a directory", "Permission denied", "I/O error",
    "Cancelled",        "Try again",      "Not supported",     "Closed",
    "Not empty",        "Too many links", "Interrupted",       "Busy",
};

char *strerror(int errnum)
{
    switch (errnum) {
    case 0:
        return "Success";
    case 1:
        return "Domain error";
    case 2:
        return "Result out of range";
    case 3:
        return "Illegal byte sequence";
    }
    if (errnum >= 33 && errnum < 33 + (int)(sizeof braam / sizeof braam[0]))
        return (char *)braam[errnum - 33];
    return "Unknown error";
}
