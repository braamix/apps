// main.go and cli.go: the entry point and the flags.
//
// goquarium, Serge Vakulenko's Go rewrite of Kirk Baucom's asciiquarium by way
// of the Python port. termbox is a ProcScreen here; the rest is the same
// aquarium.

#include "kernel/alloc.h"
#include "proc/opt.h"
#include "proc/rt.h"
#include "proc/usage.h"
#include "quarium.h"

namespace {

constexpr Str USAGE =
    "Usage:\n"
    "    asciiquarium [--classic]\n"
    "    asciiquarium --info\n"
    "    asciiquarium --version\n"
    "Options:\n"
    "    --classic    the classic fish set alone\n"
    "    --info       the help screen, and exit\n"
    "    --version    the version, and exit\n"
    "Keys: q quit, p pause, r reset, i info, ESC close info.\n";

// ASCIIQUARIUM_SEED pins the dice, for the tests. Not upstream's, which never
// seeds at all.
void seed_from_env()
{
    Str env = proc_env("ASCIIQUARIUM_SEED");
    u32 seed;
    if (!env.empty()) {
        seed = 0;
        for (usize i = 0; i < env.size(); i++) {
            if (env[i] < '0' || env[i] > '9')
                break;
            seed = seed * 10 + u32(env[i] - '0');
        }
    } else
        seed = proc_random();
    rng_seed(seed);
}

} // namespace

Task<i32> proc_main(Args args)
{
    if (help_asked(args))
        co_return co_await usage_asked(USAGE);

    bool classic = false;
    for (usize i = 1; i < args.size(); i++) {
        Str a = args[i];
        if (a == "--classic")
            classic = true;
        else if (a == "--info") {
            co_await write_all(SYS_STDOUT, info_text());
            co_return 0;
        } else if (a == "--version" || a == "-v") {
            co_await write_all(SYS_STDOUT, version_string());
            co_await write_all(SYS_STDOUT, "\n");
            co_return 0;
        } else
            co_return co_await usage_error(USAGE);
    }

    seed_from_env();

    Animation *anim = heap_new<Animation>();
    if (!anim) {
        co_await errln("asciiquarium", "the aquarium", Error::NoMemory);
        co_return 1;
    }
    anim->classic = classic;

    i32 rc = co_await quarium_run(anim);
    heap_delete(anim);
    co_return rc;
}
