// The session: what a bare `le` reopens, and where the cursor lands.
//
// Three things have to hold together for this. The history file has to be
// written at all -- it used to be created only if it already existed, so
// nothing was ever remembered. The position record has to survive the round
// trip through the file -- the path's 64-bit hash used to go out through %ld,
// which is 32 bits here, so SameFile() never matched what came back. And the
// no-argument search has to prefer this directory, or it reopens whatever the
// editor was last used on anywhere.

import { boot, le, put, keys, press, screen, status, leave, submit, tick, is, ok, get }
    from "./lelib.mjs";

await boot("lesession");
put("/home/a.txt", "home1\nhome2\nhome3\nhome4\n");
put("/home/c.txt", "cee1\ncee2\ncee3\n");
put("/tmp/b.txt", "tmp1\ntmp2\ntmp3\ntmp4\n");

// Two files in /home, a.txt left on line 3 and c.txt on line 2, so c.txt is
// this directory's most recent and a.txt the other of the pair.
le("/home/a.txt");
keys("DOWN", "DOWN");
is("a.txt, line 3", status().slice(0, 12), "Line=3     C");
leave();

le("/home/c.txt");
keys("DOWN");
leave();

// The history file exists now, and carries the position of the file just left.
const hst = get("/home/.le/history2");
is("the history file was written", String(hst !== null), "true");
is("with c.txt in front of a.txt",
   (hst.match(/\/home\/[ac]\.txt/g) || []).join(","), "/home/c.txt,/home/a.txt");

// One in /tmp, so the newest entry overall belongs to another directory.
submit("cd /tmp");
le("/tmp/b.txt");
keys("DOWN");
leave();

// Bare, back in /home: this directory's newest, at its line -- not /tmp/b.txt,
// which is what the newest-entry-anywhere rule would have picked.
submit("cd /home");
submit("clear");
le("");
tick(2);
is("a bare le reopens this directory's file", screen(1), "cee1");
is("at the line it was left on", status().slice(0, 12), "Line=2     C");

// And the other file of the pair is one switch-file away, at its own line.
press("S-F3");
tick(3);
is("switch-file reaches the other file here", screen(1), "home1");
is("also at its line", status().slice(0, 12), "Line=3     C");

// The other directory kept its own session.
leave();
submit("cd /tmp");
submit("clear");
le("");
tick(2);
is("and /tmp has a session of its own", screen(1), "tmp1");
is("at its own line", status().slice(0, 12), "Line=2     C");

ok("the history file, the position round trip, and a session per directory");
