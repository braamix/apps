// What the port kit has not got. Everything else LE calls -- the string,
// character, allocation, strtol and printf families -- is the kit's, asked for
// with PORT in CMakeLists.txt.
#include "braam.h"

// fnmatch. Backtracking on * rather than recursion, so a pattern of stars
// cannot blow the stack.
extern "C" int fnmatch(const char *pattern, const char *s, int)
{
    const char *star = nullptr, *back = nullptr;

    while (*s) {
        if (*pattern == '?') {
            pattern++, s++;
            continue;
        }
        if (*pattern == '*') {
            star = pattern++;
            back = s;
            continue;
        }
        if (*pattern == '[') {
            const char *p = pattern + 1;
            int neg       = (*p == '!' || *p == '^');
            int hit       = 0;

            if (neg)
                p++;
            for (int first = 1; *p && (*p != ']' || first); first = 0, p++) {
                if (p[1] == '-' && p[2] && p[2] != ']') {
                    if ((unsigned char)*s >= (unsigned char)p[0] &&
                        (unsigned char)*s <= (unsigned char)p[2])
                        hit = 1;
                    p += 2;
                } else if (*p == *s) {
                    hit = 1;
                }
            }
            if (*p == ']' && hit != neg) {
                pattern = p + 1;
                s++;
                continue;
            }
        } else if (*pattern == *s) {
            pattern++, s++;
            continue;
        }
        // No match here: give the last star one more character.
        if (!star)
            return FNM_NOMATCH;
        pattern = star + 1;
        s       = ++back;
    }
    while (*pattern == '*')
        pattern++;
    return *pattern ? FNM_NOMATCH : 0;
}
