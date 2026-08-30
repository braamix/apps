// The two things that need the package's own share directory: the syntax
// rules and the help. Both are found through readlink("/pkg/bin/le"), which
// is what PKGDATADIR became.

import { boot, le, put, press, screen, tick, is, ok, H } from "./lelib.mjs";

await boot("lesyntax");

const COLORS = ["black", "red", "green", "yellow", "blue", "magenta", "cyan", "white"];
const at = (x, y) => {
    const c = H.cell(H.screen(), x, y).fg;
    return COLORS[c & 7] + (c & 8 ? "+" : "");
};

put("/tmp/c.c", "int main(void)\n{\n\t/* hi */\n\treturn 0;\n}\n");
le("/tmp/c.c");

is("the file", screen(5),
`int main(void)
{
        /* hi */
        return 0;
}`);

// syntax.d/c, reached from the syntax file by `i=c`.
is("a keyword is coloured", at(0, 0), "yellow");
is("a brace is not", at(0, 1), "cyan");
is("a comment is", at(8, 2), "green");
is("and the text around it is not", at(8, 3), "yellow");

// le.hlp, in the same directory.
press("F1");
tick(3);
is("F1 opens the help", screen().split("\n")[1].includes("Help on Keys") ? "yes" : "no", "yes");
is("with the keymap in it",
   screen().split("\n").some((l) => l.includes("- line up")) ? "yes" : "no", "yes");

ok("syntax highlighting and the help, both out of share/");
