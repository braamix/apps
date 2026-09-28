#include "driver.h"

namespace {

bool number(Str s, u32 &v)
{
    if (s.empty() || s.size() > 10)
        return false;
    u64 n = 0;
    for (char c : s) {
        if (c < '0' || c > '9')
            return false;
        n = n * 10 + u64(c - '0');
    }
    if (n > ~u32(0))
        return false;
    v = u32(n);
    return true;
}

// Options clang passes, and ones wlink has no use for, that change nothing.
const Str IGNORED[] = {
    "--no-demangle", "--demangle", "--strip-debug", "--strip-all", "-s", "-S",
};

struct Parser {
    Span<const Str> w;
    usize i = 0;
    Config &cfg;
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

    bool num(Str arg, Str v, u32 &out) { return number(v, out) || fail("not a number: ", arg); }

    bool run()
    {
        for (; i < w.size() && missing.empty(); i++) {
            Str a = w[i];
            Str v;
            if (!a.starts_with("-") || a == "-") {
                cfg.inputs.push(InputArg{ a, false });
            } else if (a == "--dump") {
                cfg.dump = true;
            } else if (a == "--dump-symtab") {
                cfg.dump_symtab = true;
            } else if (a == "--trace" || a == "-t") {
                cfg.trace = true;
            } else if (a == "--allow-undefined") {
                cfg.allow_undefined = true;
            } else if (a == "--no-entry") {
                cfg.entry = Str();
            } else if (a == "--gc-sections") {
                cfg.gc_sections = true;
            } else if (a == "--no-gc-sections") {
                cfg.gc_sections = false;
            } else if (a == "--print-gc-sections") {
                cfg.print_gc_sections = true;
            } else if (a == "--no-print-gc-sections") {
                cfg.print_gc_sections = false;
            } else if (a == "--stack-first") {
                cfg.stack_first = true;
            } else if (a == "--no-stack-first") {
                cfg.stack_first = false;
            } else if (a == "--verbose") {
                cfg.verbose = true;
            } else if (a == "--dump-layout") {
                cfg.dump_layout = true;
            } else if (a == "--import-memory") {
                cfg.import_memory = true;
            } else if (take(a, "--output", v) || take(a, "-o", v)) {
                cfg.output = v;
            } else if (take(a, "-L", v)) {
                cfg.lib_dirs.push(v);
            } else if (take(a, "-l", v)) {
                cfg.inputs.push(InputArg{ v, true });
            } else if (take(a, "--entry", v) || take(a, "-e", v)) {
                cfg.entry = v;
            } else if (take(a, "--export", v)) {
                cfg.exports.push(v);
            } else if (take(a, "--undefined", v) || take(a, "-u", v)) {
                cfg.undefined.push(v);
            } else if (take(a, "--why-extract", v)) {
                cfg.why_extract = v;
            } else if (take(a, "--error-limit", v)) {
                if (!num(a, v, cfg.error_limit))
                    return false;
                diag.limit = cfg.error_limit;
            } else if (take(a, "--initial-memory", v)) {
                if (!num(a, v, cfg.initial_memory))
                    return false;
            } else if (take(a, "--max-memory", v)) {
                if (!num(a, v, cfg.max_memory))
                    return false;
            } else if (take(a, "--global-base", v)) {
                if (!num(a, v, cfg.global_base))
                    return false;
                cfg.global_base_set = true;
            } else if (take(a, "-O", v)) {
                // Strings are not merged, so every level is -O0.
            } else if (take(a, "-m", v)) {
                if (v != "wasm32")
                    return fail("unknown emulation: ", v);
            } else if (take(a, "-z", v)) {
                if (!v.starts_with("stack-size="))
                    return fail("unknown -z value: ", v);
                if (!num(a, v.substr(11), cfg.stack_size))
                    return false;
            } else {
                bool ignored = false;
                for (Str s : IGNORED)
                    ignored = ignored || a == s;
                if (!ignored)
                    return fail("unknown argument: ", a);
            }
        }
        if (!missing.empty())
            return fail("no value for ", missing);
        return true;
    }
};

} // namespace

bool parse_args(Span<const Str> words, Config &cfg, Diag &diag)
{
    Parser p{ words, 0, cfg, diag, Str() };
    return p.run();
}
