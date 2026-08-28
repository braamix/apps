// A global that has a constructor.
//
// A namespace-scope object with a non-trivial destructor needs __cxa_atexit to
// register it, and there is none: a process here is destroyed wholesale and
// runs no destructor of its own. So the object lives in storage of its own,
// which is POD and therefore trivially destructible, and is built the first
// time it is asked for -- which is the pattern CLAUDE.md names.
//
// The holder is zero-initialised in .bss, so `made` is false before anything
// runs and there is no static-initialisation order to reason about.
#pragma once

// kernel/types.h has the placement new this needs.
#include "kernel/types.h"

template <class T>
struct Global {
    alignas(T) char raw[sizeof(T)];
    bool made;

    T &get()
    {
        if (!made) {
            made = true;
            new (raw) T();
        }
        return *(T *)raw;
    }

    operator T &() { return get(); }
};
