// The error mechanism, which is the part of this port that had to be invented.
//
// Upstream's error() never returned: it longjmp'd, to the top of the command
// loop or to a catch inside visual. There is no setjmp here and none can be
// written, so error() records and the THROW macros unwind a frame at a time
// (ex_err.h). What that has to add up to is what this asserts: the message is
// upstream's, word for word; the command is abandoned; and the next command
// runs on a buffer that the failed one did not touch.

import { boot, ex, vi, press, screen, quitvi, put, is, ok } from "./exlib.mjs";

await boot("exerrors");

const FILE = "alpha\nbeta\ngamma\n";
const HEAD = `"/tmp/f" 3 lines, 17 characters`;

// Each case: a command that must fail, the message it must give, and then %p,
// which must print the buffer unchanged. If an error ever fails to unwind, the
// bad command runs on and the %p says so.
const CASES = [
    ["9p", "Not that many lines in buffer"],
    ["0p", "Nonzero address required on this command"],
    ["2,1p", "First address exceeds second"],
    ["zzz", 'Extra characters at end of "z" command'],
    ["yyy", "yyy: Not an editor command"],
    ["p x", 'Extra characters at end of "print" command'],
    ["/nosuch/", "Pattern not found"],
    ["s/nosuch/x/", "Substitute pattern match failed"],
    ["s/\\(/x/", "More \\('s than \\)'s in regular expression"],
    ["s/x\\)/y/", "More \\)'s than \\('s in regular expression"],
    ["k", "k requires following letter"],
    ["'z", "Undefined mark referenced"],
    ["d 0", "Nonzero count required"],
    ["1,2m1", "Move to a moved line"],
    // The kernel names its own errors, so this is "not found" where a Unix
    // ex said "No such file or directory".
    ["r /nosuch/file", '"/nosuch/file" not found'],
        // :set strips a leading no before looking the name up.
    ["set nosuchopt", "suchopt: No such option - 'set all' gives all option values"],
    ["set noscroll", "Option scroll is not a toggle"],
    ["|", "At end-of-file"],
    ["1,2>x", 'Extra characters at end of ">" command'],
    ["1,2j x", 'Extra characters at end of "join" command'],
];

for (const [cmd, want] of CASES) {
    put("/tmp/f", FILE);
    is(`after \`${cmd}\``, ex("/tmp/f", cmd + "\n%p\nq!\n"),
       `${HEAD}\n${want}\nalpha\nbeta\ngamma`);
}

// The same again with a good command in front, so that an error that unwound
// too far would show up as the first command's effect going missing.
put("/tmp/f", FILE);
is("a good command survives a bad one either side",
   ex("/tmp/f", "1d\nzzz\n%p\nq!\n"),
   `${HEAD}\nbeta\nExtra characters at end of "z" command\nbeta\ngamma`);

// A substitute that matches nothing is not an error inside :g -- upstream
// guards the message with !inglobal, because a global that touched only some
// of its lines is the ordinary case. Nothing is said and nothing changes.
put("/tmp/f", FILE);
is("no match inside :g", ex("/tmp/f", "g/a/s/nosuch/x/\n%p\nq!\n"),
   `${HEAD}\nalpha\nbeta\ngamma`);

ok(`${CASES.length + 2} errors: each message exact, each buffer intact`);

// The same errors from a `:` line in visual, where vcatch is set and error0()
// leaves inopen alone -- so the message goes through vclreol() on the echo
// line rather than to a pipe. A command that ran on past its own throw used to
// report twice here, and the second message ran the echo line off the bottom
// of vtube; vclreol() then raised an error of its own, which is reported
// through the same vclreol(), and the recursion took the process out with it.
const VCASES = [
    ["s", "No previous regular expression"],
    ["&", "No previous regular expression"],
    ["~", "No previous regular expression"],
    ["syntax", "syntax: Not an editor command"],
    ["s//x/", "No previous regular expression"],
    ["s/\\(/x/", "More \\('s than \\)'s in regular expression"],
    ["s/a/x/zz", 'Extra characters at end of "substitute" command'],
    ["g//d", "No previous regular expression"],
    ["g/a/s", "No previous substitute to repeat"],
];

for (const [cmd, want] of VCASES) {
    put("/tmp/f", FILE);
    vi("/tmp/f");
    press(":");
    for (const ch of cmd)
        press(ch);
    press("CR");
    const rows = screen().split("\n");
    is(`visual \`:${cmd}\``, `${rows.slice(0, 3).join("\n")}\n${rows[23] ?? ""}`,
       `alpha\nbeta\ngamma\n${want}`);
}
quitvi();

ok(`${VCASES.length} of them from a : line in visual, once each`);
