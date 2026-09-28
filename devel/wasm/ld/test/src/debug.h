// An inline template both files instantiate: one comdat, and its debug
// info in both objects.
#pragma once

template <class T>
__attribute__((noinline))
T debug_twice(T x)
{
    return x + x;
}

extern const char *debug_names[];
