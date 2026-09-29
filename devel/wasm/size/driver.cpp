#include "driver.h"

namespace {

struct Parser {
    Span<const Str> w;
    usize i = 0;
    SizeArgs &a;
    Diag &diag;

    bool fail(Str why, Str arg, Str after = Str())
    {
        Out m;
        m.put(why).put(arg).put(after);
        diag.error(m.str());
        return false;
    }

    // Whether `arg` is `flag` with a value: `--name=v` or `--name v`. False
    // with a message when the value is missing.
    bool take(Str arg, Str flag, Str &v, bool &ok)
    {
        if (arg == flag) {
            if (i + 1 < w.size())
                v = w[++i];
            else
                ok = fail(flag, ": missing argument");
            return true;
        }
        if (arg.starts_with(flag) && arg[flag.size()] == '=') {
            v = arg.substr(flag.size() + 1);
            return true;
        }
        return false;
    }

    void format(Str v)
    {
        if (v == "berkeley")
            a.size.format = SizeFormat::Berkeley;
        else if (v == "sysv")
            a.size.format = SizeFormat::Sysv;
        else if (v == "darwin")
            a.size.format = SizeFormat::Darwin;
        else
            fail("--format value should be one of: 'berkeley', 'darwin', 'sysv'", "");
    }

    void radix(Str v)
    {
        if (v == "8")
            a.size.radix = 8;
        else if (v == "10")
            a.size.radix = 10;
        else if (v == "16")
            a.size.radix = 16;
        else
            fail("--radix value should be one of: 8, 10, 16", "");
    }

    // Single letters, which may be run together: -Ax.
    bool letters(Str arg)
    {
        for (usize k = 1; k < arg.size(); k++) {
            switch (arg[k]) {
            case 'A':
                a.size.format = SizeFormat::Sysv;
                break;
            case 'B':
                a.size.format = SizeFormat::Berkeley;
                break;
            case 'm':
                a.size.format = SizeFormat::Darwin;
                break;
            case 'd':
                a.size.radix = 10;
                break;
            case 'o':
                a.size.radix = 8;
                break;
            case 'x':
                a.size.radix = 16;
                break;
            case 't':
                a.size.totals = true;
                break;
            case 'h':
                a.help = true;
                break;
            default:
                return fail("unknown argument '-", arg.substr(k, 1), "'");
            }
        }
        return true;
    }

    bool run()
    {
        for (; i < w.size(); i++) {
            Str arg = w[i];
            Str v;
            bool ok = true;
            if (!arg.starts_with("-") || arg == "-") {
                ok = a.inputs.push(arg) || fail("out of memory", "");
            } else if (arg == "--help") {
                a.help = true;
            } else if (arg == "--totals") {
                a.size.totals = true;
            } else if (arg == "--common") {
                // ELF's; wasm has no common symbols.
            } else if (take(arg, "--format", v, ok)) {
                if (ok)
                    format(v);
            } else if (take(arg, "--radix", v, ok)) {
                if (ok)
                    radix(v);
            } else if (arg.starts_with("--")) {
                ok = fail("unknown argument '", arg, "'");
            } else {
                ok = letters(arg);
            }
            if (!ok)
                return false;
        }
        if (a.inputs.empty() && !a.help)
            return fail("no input file specified", "");
        return true;
    }
};

} // namespace

bool parse_args(Span<const Str> words, SizeArgs &a, Diag &diag)
{
    Parser p{ words, 0, a, diag };
    return p.run();
}
