// The shell escapes, and the pause that lets you read what came back.
//
// The claims are what this is about: the screen and the keyboard go to the
// child and come back, and the pause has to happen while they are still gone
// -- taking the screen back is what paints over the output.

import { boot, em, put, get, press, keys, screen, row, is, ok, tick, H } from "./emlib.mjs";

await boot("embang");

put("/tmp/f", "alpha\nbeta\n");

// C-x ! runs a one-liner. Its output lands on the console, under the editor's
// last frame, because the editor gave the screen back before spawning.
em("/tmp/f");
keys("^x", "!");
H.type("echo from-the-shell");
keys("CR");
tick(6);
is("the child's output and the pause", screen().split("\n").slice(-2).join(" / "),
   "from-the-shell / (End)");

// Any key resumes, and the screen comes back.
keys("CR");
tick(3);
is("and the buffer is back", [row(0), row(1)].join("\n"), "alpha\nbeta");

// The buffer is untouched by the escape.
keys("^x", "^s");
tick(2);
is("the file is what it was", get("/tmp/f"), "alpha\nbeta\n");

// C-x @ reads a command's output into a window of its own, in view mode.
em("/tmp/f");
keys("^x", "@");
H.type("echo one; echo two");
keys("CR");
tick(6);
is("C-x @ opens a window on the output", row(10).slice(0, 44),
   "-- uEmacs/Pk 4.0.15: command (View utf-8) --");
is("and leaves the file in the other half", [row(11), row(12)].join("\n"), "alpha\nbeta");

// C-x # filters the buffer through a command. Upstream drove the filter
// through two pipes, one of them written by a second forked copy of the
// editor; a temp file on each side is what it is here, because one task
// cannot park on both ends of a pipeline.
//
// The command is one Braam has: /bin is forty-odd programs and not a Unix,
// so "sort" and "tr" are not among them.
em("/tmp/f");
keys("^x", "#");
H.type("grep alpha");
keys("CR");
tick(8);
is("C-x # filters it", [row(0), row(1)].join("|"), "alpha|");
is("and the temp files are gone",
   [get("fltinp"), get("fltout")].join(","), ",");

ok("the escapes, the pause, the pipe and the filter");
