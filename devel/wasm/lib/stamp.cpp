#include "stamp.h"

#include "wasm.h"

namespace {

u32 get_u32le(const u8 *p)
{
    return u32(p[0]) | u32(p[1]) << 8 | u32(p[2]) << 16 | u32(p[3]) << 24;
}

} // namespace

bool find_stamp(const Vec<Section> &sections, Stamp &s)
{
    for (const Section &x : sections) {
        if (x.id != wasm::SEC_CUSTOM || x.name != "braam")
            continue;
        if (x.body.size() < 20)
            return false;
        const u8 *p = x.body.data();
        s           = Stamp{ get_u32le(p), get_u32le(p + 4), get_u32le(p + 8), get_u32le(p + 12),
                             get_u32le(p + 16) };
        return s.magic == BRAAM_MAGIC;
    }
    return false;
}

void emit_stamp(Emit &e, const Stamp &s)
{
    Str name = "braam";
    e.byte(wasm::SEC_CUSTOM);
    e.uleb(uleb_size(name.size()) + name.size() + 20);
    e.name(name);
    const u32 words[] = { s.magic, s.abi, s.flags, s.initial_pages, s.max_pages };
    for (u32 v : words)
        e.u32le(v);
}
