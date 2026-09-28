#include "driver.h"

namespace {

struct Parser {
    Span<const Str> w;
    usize i = 0;
    StripArgs &a;
    Diag &diag;
    Str missing;

    bool fail(Str why, Str arg)
    {
        Out m;
        m.put(why).put(arg);
        diag.error(m.str());
        return false;
    }

    // Whether `arg` is `flag` with a value: `--name=v`, `--name v`, `-xv` or
    // `-x v`. A flag with its value missing matches, and sets `missing`.
    bool take(Str arg, Str flag, Str &v)
    {
        if (arg == flag) {
            if (i + 1 < w.size())
                v = w[++i];
            else
                missing = arg;
            return true;
        }
        bool eq    = flag.starts_with("--") && arg.starts_with(flag) && arg[flag.size()] == '=';
        bool glued = !flag.starts_with("--") && arg.starts_with(flag);
        if (eq || glued)
            v = arg.substr(flag.size() + (eq ? 1 : 0));
        return eq || glued;
    }

    bool run()
    {
        bool all = false, debug = false, options = true;
        if (!a.strip.keep.push("braam"))
            return fail("out of memory", "");
        for (; i < w.size() && missing.empty(); i++) {
            Str arg = w[i];
            Str v;
            bool ok = true;
            if (!options || !arg.starts_with("-") || arg == "-") {
                ok = a.inputs.push(arg);
            } else if (arg == "--") {
                options = false;
            } else if (arg == "-h" || arg == "--help") {
                a.help = true;
            } else if (arg == "-s" || arg == "--strip-all") {
                all = true;
            } else if (arg == "-g" || arg == "-S" || arg == "-d" || arg == "--strip-debug") {
                debug = true;
            } else if (take(arg, "--remove-section", v) || take(arg, "-R", v)) {
                ok = missing.empty() ? a.strip.remove.push(v) : true;
            } else if (take(arg, "--keep-section", v)) {
                ok = missing.empty() ? a.strip.keep.push(v) : true;
            } else if (take(arg, "-o", v)) {
                a.output = v;
            } else {
                return fail("unknown argument: ", arg);
            }
            if (!ok)
                return fail("out of memory", "");
        }
        if (!missing.empty())
            return fail("no value for ", missing);
        if (a.help)
            return true;
        // llvm-strip's default is --strip-all; -g alone asks for less.
        a.strip.debug_only = debug && !all;
        if (a.inputs.empty())
            return fail("no input file specified", "");
        if (!a.output.empty() && a.inputs.size() > 1)
            return fail("multiple input files cannot be used in combination with -o", "");
        Out m;
        if (!check_strip_config(a.strip, m))
            return fail(m.str(), "");
        return true;
    }
};

} // namespace

bool parse_args(Span<const Str> words, StripArgs &a, Diag &diag)
{
    Parser p{ words, 0, a, diag, Str() };
    return p.run();
}
