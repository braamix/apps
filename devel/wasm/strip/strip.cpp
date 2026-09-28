#include "strip.h"

#include "emit.h"
#include "module.h"
#include "wasm.h"

using namespace wasm;

namespace {

bool listed(const Vec<Str> &names, Str name)
{
    for (Str n : names)
        if (n == name)
            return true;
    return false;
}

// llvm-strip's rules: a standard section is never removed; --keep-section
// wins over everything; -R removes; --strip-all takes every other custom
// section, and -g only debug info and its relocations.
bool removed(const StripConfig &c, const Section &s)
{
    if (s.id != SEC_CUSTOM || listed(c.keep, s.name))
        return false;
    if (listed(c.remove, s.name) || !c.debug_only)
        return true;
    return s.name.starts_with(".debug") || s.name.starts_with("reloc..debug");
}

// An object's sections are counted by index, so a removed one leaves an
// empty custom section in its place, its size padded as clang pads it.
void placeholder(Emit &e)
{
    Str name = ".objcopy.removed";
    e.byte(SEC_CUSTOM);
    e.uleb5(uleb_size(name.size()) + name.size());
    e.name(name);
}

bool is_bitcode(Bytes f)
{
    return f.size() >= 4 && f[0] == 'B' && f[1] == 'C' && f[2] == 0xc0 && f[3] == 0xde;
}

} // namespace

bool check_strip_config(const StripConfig &c, Out &err)
{
    for (Str n : c.remove) {
        if (n == "braam") {
            err.put("removing the braam section makes the program unrunnable");
            return false;
        }
        for (u8 id = SEC_TYPE; id <= SEC_TAG; id++)
            if (n == section_name(id)) {
                err.put("removing the ").put(n).put(" section makes the module invalid");
                return false;
            }
    }
    return true;
}

bool strip_module(Str name, Bytes file, const StripConfig &c, Vec<u8> &out, Out &err)
{
    if (is_bitcode(file)) {
        err.put(name).put(": LLVM bitcode (from -flto), not a wasm module");
        return false;
    }
    Vec<Section> sections;
    if (!read_module(name, file, sections, err))
        return false;
    bool object = is_object(sections);

    // Sections are contiguous, so each runs from the end of the one before
    // it; a kept one is copied whole, its size field as it was.
    Emit e{ out };
    if (!out.reserve(file.size()))
        e.oom = true;
    e.bytes(file.subspan(0, 8));
    usize at = 8;
    for (const Section &s : sections) {
        usize end = s.file_off + s.body.size();
        if (!removed(c, s))
            e.bytes(file.subspan(at, end - at));
        else if (object)
            placeholder(e);
        at = end;
    }
    if (e.oom) {
        err.put(name).put(": out of memory");
        return false;
    }
    return true;
}
