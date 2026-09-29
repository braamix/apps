#include "driver.h"

namespace {

bool fail(Diag &diag, Str why, Str arg, Str after)
{
    Out m;
    m.put(why).put(arg).put(after);
    diag.error(m.str());
    return false;
}

// Single letters, which may be run together: -dr.
bool letters(Str arg, DisasmArgs &a, Diag &diag)
{
    for (usize k = 1; k < arg.size(); k++) {
        switch (arg[k]) {
        case 'd':
            a.c.data = false;
            break;
        case 'D':
            a.c.data = true;
            break;
        case 'r':
            a.c.relocs = true;
            break;
        case 'C':
            a.c.demangle = true;
            break;
        case 'h':
            a.help = true;
            break;
        default:
            return fail(diag, "unknown argument '-", arg.substr(k, 1), "'");
        }
    }
    return true;
}

bool word(Str arg, DisasmArgs &a, Diag &diag)
{
    if (arg == "--disassemble")
        a.c.data = false;
    else if (arg == "--disassemble-all")
        a.c.data = true;
    else if (arg == "--reloc")
        a.c.relocs = true;
    else if (arg == "--demangle")
        a.c.demangle = true;
    else if (arg == "--no-demangle")
        a.c.demangle = false;
    else if (arg == "--no-show-raw-insn")
        a.c.raw = false;
    else if (arg == "--show-raw-insn")
        a.c.raw = true;
    else if (arg == "--help")
        a.help = true;
    else
        return fail(diag, "unknown argument '", arg, "'");
    return true;
}

} // namespace

bool parse_args(Span<const Str> words, DisasmArgs &a, Diag &diag)
{
    for (Str arg : words) {
        bool ok;
        if (!arg.starts_with("-") || arg == "-")
            ok = a.inputs.push(arg) || fail(diag, "out of memory", Str(), Str());
        else if (arg.starts_with("--"))
            ok = word(arg, a, diag);
        else
            ok = letters(arg, a, diag);
        if (!ok)
            return false;
    }
    if (a.inputs.empty() && !a.help)
        return fail(diag, "no input file specified", Str(), Str());
    return true;
}
