#include "emit.h"

#include "wasm.h"

void Emit::u32le(u32 x)
{
    for (u32 k = 0; k < 4; k++)
        byte(u8(x >> (8 * k)));
}

void Emit::uleb(u32 x)
{
    do {
        u8 b = x & 0x7f;
        x >>= 7;
        byte(u8(b | (x ? 0x80 : 0)));
    } while (x);
}

void Emit::uleb64(u64 x)
{
    do {
        u8 b = x & 0x7f;
        x >>= 7;
        byte(u8(b | (x ? 0x80 : 0)));
    } while (x);
}

void Emit::uleb5(u32 x)
{
    for (u32 k = 0; k < 5; k++)
        byte(u8(((x >> (7 * k)) & 0x7f) | (k < 4 ? 0x80 : 0)));
}

void Emit::sleb(i32 x)
{
    for (;;) {
        u8 b = x & 0x7f;
        x >>= 7;
        bool done = (x == 0 && !(b & 0x40)) || (x == -1 && (b & 0x40));
        byte(u8(b | (done ? 0 : 0x80)));
        if (done)
            return;
    }
}

void Emit::sleb64(i64 x)
{
    for (;;) {
        u8 b = x & 0x7f;
        x >>= 7;
        bool done = (x == 0 && !(b & 0x40)) || (x == -1 && (b & 0x40));
        byte(u8(b | (done ? 0 : 0x80)));
        if (done)
            return;
    }
}

void Emit::bytes(Bytes b)
{
    if (!v.reserve(v.size() + b.size()))
        oom = true;
    for (u8 c : b)
        byte(c);
}

void Emit::name(Str s)
{
    uleb(s.size());
    bytes(Bytes(reinterpret_cast<const u8 *>(s.data()), s.size()));
}

u32 uleb_size(u32 v)
{
    u32 n = 1;
    while (v >>= 7)
        n++;
    return n;
}

void emit_section(Emit &e, u8 id, Bytes body)
{
    e.byte(id);
    e.uleb(body.size());
    e.bytes(body);
}

void emit_custom(Emit &e, Str name, Bytes body)
{
    e.byte(wasm::SEC_CUSTOM);
    e.uleb(uleb_size(name.size()) + name.size() + body.size());
    e.name(name);
    e.bytes(body);
}
