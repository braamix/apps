// UTF-8: a line is bytes, a character is a codepoint, and a column is one of
// those. Upstream dropped everything over 0177 -- TRIM was 0177 and QUOTE was
// the bit above it -- so this is the case that says the two are separated now.
//
// The harness needs nothing special: type() sends whole codepoints and the
// grid holds one per cell, so a Cyrillic assertion is written literally.

import { boot, vi, ex, press, screen, cursor, put, get, quitvi, is, ok, H } from "./exlib.mjs";

await boot("viutf8");

const RU = "привет мир";

/* ------------------------------------------------------- command mode */

// The bytes come back exactly as they went in: nothing is masked on the way
// through the buffer, and :p writes the same bytes it read.
put("/tmp/f", RU + "\n");
is("ex round trip", ex("/tmp/f", "1p\n"), `"/tmp/f" 1 line, 20 characters\n${RU}\n`);

put("/tmp/f", RU + "\n");
ex("/tmp/f", "1s/мир/свет/\nw\n");
is("a substitution keeps the rest", get("/tmp/f"), "привет свет\n");

// A file of mixed widths, byte for byte after a write that changes nothing.
const MIX = "aбв\tгg\nl2 ascii\nʃ ɑ ə\n";
put("/tmp/g", MIX);
ex("/tmp/g", "w\n");
is("write back unchanged", get("/tmp/g"), MIX);

/* ------------------------------------------------------------- display */

function edit(keys, want, what, rows = 1) {
    put("/tmp/f", RU + "\n");
    vi("/tmp/f", keys);
    is(what, screen(rows), want);
}

edit([], RU, "the line as it is");

// The caret counts characters, not bytes: "привет мир" is ten of the first
// and nineteen of the second.
put("/tmp/f", RU + "\n");
vi("/tmp/f", ["$"]);
is("the caret at $", cursor(), "9,0");

put("/tmp/f", RU + "\n");
vi("/tmp/f", ["l", "l"]);
is("the caret after two motions", cursor(), "2,0");

/* ------------------------------------------------------------- editing */

// x takes a whole character, not a byte.
edit(["x"], "ривет мир", "x takes a character");
edit(["l", "l", "x"], "првет мир", "x after two motions");
edit(["x", "x", "x"], "вет мир", "x three times");

// So does X, and Backspace, which is X with the insertion left open.
edit(["l", "l", "X"], "пивет мир", "X takes the one before");
edit(["l", "l", "i", "BACKSPACE", "ESC"], "пивет мир", "backspace takes a character");

// r replaces one character with one of a different byte length, both ways.
edit(["r", "ю"], "юривет мир", "r with a two-byte character");
edit(["r", "z"], "zривет мир", "r narrower");
put("/tmp/f", "abc\n");
vi("/tmp/f", ["r", "ю"]);
is("r wider", screen(1), "юbc");

// dw over a Cyrillic run: the ASCII word classes make it one blank-delimited
// word, which is the documented limitation, but it must not split a character.
edit(["d", "w"], "мир", "dw does not split a character");

/* ------------------------------------------------------------- typing */

edit(["i", "щ", "ESC"], "щпривет мир", "a typed character");
edit(["i", "щы", "ESC"], "щыпривет мир", "two of them");
edit(["A", " да", "ESC"], RU + " да", "appended");
edit(["i", "aщb", "ESC"], "aщbпривет мир", "mixed with ASCII");

// And what was typed can be rubbed out again, one character at a time.
edit(["i", "щы", "BACKSPACE", "ESC"], "щпривет мир", "typed then rubbed out");

/* ------------------------------------------------- malformed input */

// A stray 0xFF can never appear in UTF-8 -- put() would encode it as U+00FF,
// so the bytes go in raw. It renders as U+FFFD and does not eat the line.
H.store.files.set("/tmp/f", new Uint8Array([0x61, 0xff, 0x62, 0x0a]));
vi("/tmp/f", []);
is("a malformed byte", screen(1), "a�b");

// A truncated sequence is the other malformed shape: a lead with no tail.
H.store.files.set("/tmp/f", new Uint8Array([0x61, 0xd0, 0x62, 0x0a]));
vi("/tmp/f", []);
is("a truncated sequence", screen(1), "a�b");

/* ------------------------------------------------ the limitations */

// Stated in README.md, and pinned here so they stay deliberate: the regex
// engine is byte-at-a-time, so . is one byte and does not match a character.
quitvi(); /* ex cannot be submitted while visual holds the screen */
put("/tmp/f", RU + "\n");
is("a literal pattern matches", ex("/tmp/f", "/привет/p\n"),
   `"/tmp/f" 1 line, 20 characters\n${RU}\n`);

put("/tmp/f", RU + "\n");
ex("/tmp/f", "1s/п.ивет/X/\nw\n");
is(". does not match a character", get("/tmp/f"), RU + "\n");

quitvi();
ok("round trip, display, x/X/r/backspace, typing, U+FFFD and the limits");
