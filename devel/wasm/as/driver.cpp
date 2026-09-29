#include "driver.h"

namespace {

bool fail(Diag &diag, Str why, Str arg)
{
    Out m;
    m.put(why).put(arg);
    diag.error(m.str());
    return false;
}

} // namespace

bool parse_args(Span<const Str> w, AsArgs &a, Diag &diag)
{
    bool options = true;
    for (usize i = 0; i < w.size(); i++) {
        Str arg = w[i];
        bool ok = true;
        if (!options || !arg.starts_with("-") || arg == "-") {
            ok = a.inputs.push(arg);
        } else if (arg == "--") {
            options = false;
        } else if (arg == "-h" || arg == "--help") {
            a.help = true;
        } else if (arg == "--module") {
            a.as.module = true;
        } else if (arg == "-o") {
            if (i + 1 == w.size())
                return fail(diag, "no value for ", arg);
            a.output = w[++i];
        } else if (arg.starts_with("-o")) {
            a.output = arg.substr(2);
        } else {
            return fail(diag, "unknown argument: ", arg);
        }
        if (!ok)
            return fail(diag, "out of memory", "");
    }
    if (a.help)
        return true;
    if (a.inputs.empty())
        return fail(diag, "no input file specified", "");
    if (!a.output.empty() && a.inputs.size() > 1)
        return fail(diag, "multiple input files cannot be used in combination with -o", "");
    return true;
}

bool output_name(const AsArgs &a, Str input, String &out, Diag &diag)
{
    Str name = a.output;
    if (name.empty()) {
        Str base = input;
        for (usize k = input.size(); k > 0; k--)
            if (input[k - 1] == '/') {
                base = input.substr(k);
                break;
            }
        // A leading dot is part of the name, not an extension.
        for (usize k = base.size(); k > 1; k--)
            if (base[k - 1] == '.') {
                base = base.substr(0, k - 1);
                break;
            }
        if (!out.assign(base) || !out.append(a.as.module ? ".wasm" : ".o"))
            return fail(diag, "out of memory", "");
        name = out.str();
    } else if (!out.assign(name)) {
        return fail(diag, "out of memory", "");
    }
    if (name == input)
        return fail(diag, "output would overwrite the input: ", input);
    return true;
}
