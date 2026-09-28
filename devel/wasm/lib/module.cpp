#include "module.h"

#include "cursor.h"
#include "wasm.h"

using namespace wasm;

namespace {

// Where a standard section may stand. TAG sits between MEMORY and GLOBAL,
// DATACOUNT between ELEM and CODE.
u32 rank(u8 id)
{
    static const u8 RANK[] = { 0, 1, 2, 3, 4, 5, 7, 8, 9, 10, 12, 13, 11, 6 };
    return RANK[id];
}

bool fail(Out &err, Str name, Str why)
{
    err.put(name).put(": ").put(why);
    return false;
}

} // namespace

bool read_module(Str name, Bytes f, Vec<Section> &sections, Out &err)
{
    if (f.size() < 8 || f[0] != MAGIC[0] || f[1] != MAGIC[1] || f[2] != MAGIC[2] ||
        f[3] != MAGIC[3])
        return fail(err, name, "not a wasm module");
    if (f[4] != VERSION || f[5] || f[6] || f[7])
        return fail(err, name, "wasm version other than 1");

    Cursor c(f);
    c.take(8);
    u32 last = 0;
    while (c.ok() && !c.done()) {
        usize at = c.at();
        Section s{};
        s.id     = c.byte();
        u32 size = c.uleb();
        if (c.ok() && size > c.left())
            c.fail("runs past the end of the file", at);
        Bytes b = c.take(size);
        if (!c.ok())
            break;
        if (s.id > SEC_TAG) {
            c.fail("unknown section id", at);
            break;
        }
        if (s.id == SEC_CUSTOM) {
            Cursor n(b);
            s.name = n.name();
            if (!n.ok()) {
                c.fail("custom section name runs past the section", at);
                break;
            }
            s.body     = b.subspan(n.at());
            s.file_off = b.data() - f.data() + n.at();
        } else {
            if (rank(s.id) <= last) {
                c.fail("standard section out of order or repeated", at);
                break;
            }
            last       = rank(s.id);
            s.name     = section_name(s.id);
            s.body     = b;
            s.file_off = b.data() - f.data();
        }
        if (!sections.push(s))
            return fail(err, name, "out of memory");
    }
    if (!c.ok()) {
        err.put(name).put(": section at file offset 0x").hex(u32(c.where())).put(": ");
        err.put(c.why());
        return false;
    }
    return true;
}

bool is_object(const Vec<Section> &sections)
{
    for (const Section &s : sections)
        if (s.id == SEC_CUSTOM && s.name == "linking")
            return true;
    return false;
}
