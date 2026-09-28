// Text built up in a String: messages, and --dump's report.
#pragma once

#include "kernel/str.h"
#include "kernel/string.h"
#include "kernel/types.h"

struct Out {
    String s;
    bool oom = false; // an append failed; the text is short

    Out &put(Str t)
    {
        if (!s.append(t))
            oom = true;
        return *this;
    }

    Out &put(char c)
    {
        if (!s.push(c))
            oom = true;
        return *this;
    }

    Out &num(u32 v)
    {
        char t[10];
        usize k = 0;
        do {
            t[k++] = char('0' + v % 10);
            v /= 10;
        } while (v);
        while (k)
            put(t[--k]);
        return *this;
    }

    Out &snum(i32 v)
    {
        if (v < 0)
            return put('-').num(u32(-(v + 1)) + 1);
        return num(u32(v));
    }

    // Lower-case hex, zero-padded to `digits`.
    Out &hex(u32 v, u32 digits = 1)
    {
        char t[8];
        u32 k = 0;
        do {
            t[k++] = "0123456789abcdef"[v & 0xf];
            v >>= 4;
        } while (v);
        for (u32 i = k; i < digits; i++)
            put('0');
        while (k)
            put(t[--k]);
        return *this;
    }

    Out &left(Str t, usize w)
    {
        put(t);
        for (usize i = t.size(); i < w; i++)
            put(' ');
        return *this;
    }

    // Lower-case hex, right-aligned in a field of `w`.
    Out &rhex(u32 v, usize w)
    {
        u32 k = 0;
        for (u32 x = v; k == 0 || x; x >>= 4)
            k++;
        for (usize i = k; i < w; i++)
            put(' ');
        return hex(v);
    }

    // Decimal, left-aligned in a field of `w`.
    Out &lnum(u32 v, usize w)
    {
        usize k = 0;
        for (u32 x = v; k == 0 || x; x /= 10)
            k++;
        num(v);
        for (usize i = k; i < w; i++)
            put(' ');
        return *this;
    }

    Str str() const { return s.str(); }

    void clear()
    {
        s.clear();
        oom = false;
    }
};
