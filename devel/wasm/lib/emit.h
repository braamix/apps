// Encoders into a growing byte vector. Out of memory is sticky, and is
// checked once at the end rather than after every write.
#pragma once

#include "kernel/span.h"
#include "kernel/str.h"
#include "kernel/types.h"
#include "kernel/vec.h"

struct Emit {
    Vec<u8> &v;
    bool oom = false;

    void byte(u8 b)
    {
        if (!v.push(b))
            oom = true;
    }

    void u32le(u32 x);
    void uleb(u32 x);
    void uleb64(u64 x);
    void uleb5(u32 x); // padded to five bytes, as an object's sizes are
    void uleb10(u64 x);
    void sleb(i32 x);
    void sleb5(i32 x);
    void sleb10(i64 x);
    void sleb64(i64 x);
    void bytes(Bytes b);
    void name(Str s); // uleb length, then the bytes
};

// How many bytes uleb(v) writes.
u32 uleb_size(u32 v);

// A section: id, size, body. A custom one: its name goes first.
void emit_section(Emit &e, u8 id, Bytes body);
void emit_custom(Emit &e, Str name, Bytes body);
