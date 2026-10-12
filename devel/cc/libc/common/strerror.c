/*
 * strerror — map an error number to a message string (C11 §7.24.6.2).
 *
 * Returns a pointer to a static, read-only message for errnum.  The error set
 * matches <errno.h> (EDOM=1 .. EIO=6); unknown codes yield a generic message.
 * Messages are upper case, which the BESM-6 output charset (KOI-7) can print.
 */
#include <string.h>

char *strerror(int errnum)
{
    switch (errnum) {
    case 0:
        return "SUCCESS";
    case 1:
        return "DOMAIN ERROR"; /* EDOM   */
    case 2:
        return "RESULT OUT OF RANGE"; /* ERANGE */
    case 3:
        return "ILLEGAL BYTE SEQUENCE"; /* EILSEQ */
    case 4:
        return "INVALID ARGUMENT"; /* EINVAL */
    case 5:
        return "OUT OF MEMORY"; /* ENOMEM */
    case 6:
        return "I/O ERROR"; /* EIO    */
    default:
        return "UNKNOWN ERROR";
    }
}
