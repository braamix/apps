#include "driver.h"

namespace {

struct Parser {
    Span<const Str> w;
    usize i = 0;
    NmArgs &a;
    Diag &diag;
    Str format_v, radix_v, bits_v; // each option's last value

    bool fail(Str why, Str arg = Str(), Str after = Str())
    {
        Out m;
        m.put(why).put(arg).put(after);
        diag.error(m.str());
        return false;
    }

    // The value of an option that takes one: glued on (`-tx`, `--radix=x`),
    // or the next word.
    bool value(Str name, Str glued, bool has, Str &v)
    {
        if (has) {
            v = glued;
            return true;
        }
        if (i + 1 < w.size()) {
            v = w[++i];
            return true;
        }
        return fail(name, ": missing argument");
    }

    // Each value is checked once, the last given. An unknown one leaves the
    // default that llvm-nm's own variable starts with: bsd, and decimal.
    void format(Str v)
    {
        if (v.empty())
            return;
        NmFormat &f = a.nm.format;
        if (v == "bsd")
            f = NmFormat::Bsd;
        else if (v == "posix")
            f = NmFormat::Posix;
        else if (v == "sysv")
            f = NmFormat::Sysv;
        else if (v == "darwin")
            f = NmFormat::Darwin;
        else if (v == "just-symbols")
            f = NmFormat::JustSymbols;
        else {
            f = NmFormat::Bsd;
            fail("--format value should be one of: bsd, posix, sysv, darwin, just-symbols");
        }
    }

    void radix(Str v)
    {
        if (v.empty())
            return;
        if (v == "o")
            a.nm.radix = 8;
        else if (v == "d")
            a.nm.radix = 10;
        else if (v == "x")
            a.nm.radix = 16;
        else {
            a.nm.radix = 10;
            fail("--radix value should be one of: 'o' (octal), 'd' (decimal), 'x' (hexadecimal)");
        }
    }

    // -X picks 32- or 64-bit objects on AIX; wasm32 is always read.
    void bits(Str v)
    {
        if (!v.empty() && v != "32" && v != "64" && v != "32_64" && v != "any")
            fail("-X value should be one of: 32, 64, 32_64, (default) any");
    }

    // Single letters, which may be run together: -gU. -f, -t and -X take the
    // rest of the word as their value, or the next word.
    bool letters(Str arg)
    {
        NmConfig &c = a.nm;
        for (usize k = 1; k < arg.size(); k++) {
            char l   = arg[k];
            Str rest = arg.substr(k + 1);
            Str v;
            switch (l) {
            case 'a': // debugger symbols: wasm has none apart
                break;
            case 'A':
            case 'o':
                c.print_file = true;
                break;
            case 'B':
                format_v = "bsd";
                break;
            case 'C':
                c.demangle = true;
                break;
            case 'D':
                c.dynamic = true;
                break;
            case 'g':
                c.extern_only = true;
                break;
            case 'h':
                a.help = true;
                break;
            case 'j':
                format_v = "just-symbols";
                break;
            case 'm':
                format_v = "darwin";
                break;
            case 'M':
                c.print_armap = true;
                break;
            case 'n':
            case 'v':
                c.numeric_sort = true;
                break;
            case 'p':
                c.no_sort = true;
                break;
            case 'P':
                format_v = "posix";
                break;
            case 'r':
                c.reverse_sort = true;
                break;
            case 'S':
                c.print_size = true;
                break;
            case 'u':
                c.undefined_only = true;
                break;
            case 'U':
                c.defined_only = true;
                break;
            case 'V':
                a.version = true;
                break;
            case 'W':
                c.no_weak = true;
                break;
            case 'f':
            case 't':
            case 'X': {
                char name[2] = { '-', l };
                if (!value(Str(name, 2), rest, !rest.empty(), v))
                    return false;
                (l == 'f' ? format_v : l == 't' ? radix_v : bits_v) = v;
                return true;
            }
            default:
                return fail("unknown argument '-", arg.substr(k, 1), "'");
            }
        }
        return true;
    }

    // A long option with a value: `--name=v` or `--name v`.
    bool with_value(Str arg, Str name, Str &v, bool &ok)
    {
        if (arg == name) {
            ok = value(name, Str(), false, v);
            return true;
        }
        if (arg.starts_with(name) && arg[name.size()] == '=') {
            v = arg.substr(name.size() + 1);
            return true;
        }
        return false;
    }

    bool word(Str arg)
    {
        NmConfig &c = a.nm;
        Str v;
        bool ok = true;
        if (arg == "--debug-syms" || arg == "--special-syms" || arg == "--no-llvm-bc" ||
            arg == "--without-aliases") {
            // Nothing in wasm that these would show or hide.
        } else if (arg == "--defined-only") {
            c.defined_only = true;
        } else if (arg == "--demangle") {
            c.demangle = true;
        } else if (arg == "--no-demangle") {
            c.demangle = false;
        } else if (arg == "--dynamic") {
            c.dynamic = true;
        } else if (arg == "--export-symbols") {
            c.export_symbols = true;
        } else if (arg == "--extern-only") {
            c.extern_only = true;
        } else if (arg == "--help") {
            a.help = true;
        } else if (arg == "--no-sort") {
            c.no_sort = true;
        } else if (arg == "--no-weak") {
            c.no_weak = true;
        } else if (arg == "--numeric-sort") {
            c.numeric_sort = true;
        } else if (arg == "--print-armap") {
            c.print_armap = true;
        } else if (arg == "--print-file-name") {
            c.print_file = true;
        } else if (arg == "--print-size") {
            c.print_size = true;
        } else if (arg == "--quiet") {
            c.quiet = true;
        } else if (arg == "--reverse-sort") {
            c.reverse_sort = true;
        } else if (arg == "--size-sort") {
            c.size_sort = true;
        } else if (arg == "--undefined-only") {
            c.undefined_only = true;
        } else if (arg == "--version") {
            a.version = true;
        } else if (arg == "--just-symbol-name") {
            format_v = "just-symbols";
        } else if (arg == "--portability") {
            format_v = "posix";
        } else if (with_value(arg, "--format", v, ok)) {
            if (ok)
                format_v = v;
        } else if (with_value(arg, "--radix", v, ok)) {
            if (ok)
                radix_v = v;
        } else {
            ok = fail("unknown argument '", arg, "'");
        }
        return ok;
    }

    bool run()
    {
        for (; i < w.size(); i++) {
            Str arg = w[i];
            bool ok;
            if (!arg.starts_with("-") || arg == "-")
                ok = a.inputs.push(arg) || fail("out of memory");
            else if (arg.starts_with("--"))
                ok = word(arg);
            else
                ok = letters(arg);
            if (!ok)
                return false;
        }
        if (a.inputs.empty() && !a.inputs.push("a.out"))
            return fail("out of memory");
        format(format_v);
        radix(radix_v);
        bits(bits_v);
        nm_settle(a.nm);
        return true;
    }
};

} // namespace

bool parse_args(Span<const Str> words, NmArgs &a, Diag &diag)
{
    Parser p{ words, 0, a, diag, Str(), Str(), Str() };
    return p.run();
}
