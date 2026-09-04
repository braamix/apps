// info.go and version.go: the help screen, the overlay, and the version line.

#include "kernel/alloc.h"
#include "kernel/version.h"
#include "quarium.h"

namespace {

// goquarium's, which is the Perl original's numbering.
constexpr Str VERSION = "2.2.0";

constexpr Str INFO_LINES[] = {
    "╔═══════════════════════════════════════════════════════════════════════╗",
    "║                                                                       ║",
    "║         Asciiquarium 2.2.0 - ASCII Art Aquarium Animation             ║",
    "║                                                                       ║",
    "╚═══════════════════════════════════════════════════════════════════════╝",
    "",
    "  Q/q quit   P/p pause   R/r reset   I/i info   ESC close info",
    "",
    "  Press I or ESC to return to aquarium...",
};

// One buffer for the version line, built once and viewed after.
String *version_line;

} // namespace

Str info_text()
{
    return R"AQ(
╔════════════════════════════════════════════════════════════════════╗
║                                                                    ║
║   Asciiquarium 2.2.0 - ASCII Art Aquarium Animation                ║
║                                                                    ║
╚════════════════════════════════════════════════════════════════════╝

An aquarium/sea animation in ASCII art for your terminal!

CONTROLS:
  Q or q  - Quit the aquarium
  P or p  - Pause/unpause animation
  R or r  - Redraw and respawn entities
  I or i  - Show/hide info screen (press I or ESC to return)
)AQ";
}

Span<const Str> info_lines()
{
    return Span<const Str>(INFO_LINES, sizeof INFO_LINES / sizeof INFO_LINES[0]);
}

// Upstream names the Go runtime here; this one names Braam.
Str version_string()
{
    if (!version_line) {
        version_line = heap_new<String>();
        if (!version_line)
            return "asciiquarium";
        version_line->append("asciiquarium/");
        version_line->append(VERSION);
        version_line->append(" Braam/");
        version_line->append(BRAAM_VERSION);
    }
    return version_line->str();
}
