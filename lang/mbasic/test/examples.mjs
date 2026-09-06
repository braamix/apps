// The nineteen programs in examples/, each LOADed and RUN once.
//
// LOAD rather than `mb <file>`, because a file argument is read as if typed and
// there is no implicit RUN -- a file-argument run would tokenize the program and
// stop on the banner. session() joins its lines into one stdin, so after RUN the
// program's own INPUTs read what follows: that is what `answers` is.
//
// Each entry is its own mb process, so RND restarts from its fixed seed and one
// example's stream cannot shift another's. The answers were chosen by blessing
// and reading the transcript; a game's are the moves that particular sequence
// calls for, which is why they look arbitrary.

import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

import { boot, session, body, golden, put, die, ok } from "./mblib.mjs";

const H = await boot("examples");

const HERE = dirname(fileURLToPath(import.meta.url));

const CASES = [
    // No input at all.
    ["hello.bas", []],
    ["fibonacci.bas", []],
    ["sort.bas", []],
    // One prompt, or a handful.
    ["stars.bas", ["5"]],
    ["primes.bas", ["50"]],
    ["sinewave.bas", ["N"]],
    ["factorial.bas", ["10", "N"]],
    ["math.bas", ["2", "N"]],
    ["strings.bas", ["HELLO WORLD", "N"]],
    ["tempconv.bas", ["1", "100", "N"]],
    ["calendar.bas", ["2026", "9", "N"]],
    // The games, whose moves answer one particular RND sequence: the number is
    // 54, the word is MONITOR, the code is 4323, the wumpus is in room 9.
    ["guess.bas", ["50", "75", "62", "56", "53", "54", "N"]],
    // Ten turns of free fall, then the brake -- 250 units of fuel against a
    // velocity of 50 and a thousand feet, with the last few turns holding V at
    // walking pace. The play-again N is the last line.
    ["lunar.bas", ["0", "0", "0", "0", "0", "0", "0", "0", "0", "0",
                   "30", "30", "22", "16", "12", "10", "8", "8", "6", "6",
                   "6", "5", "5", "5", "N"]],
    ["slots.bas", ["10", "10", "0"]],
    // E misses, the second E is refused as already guessed, the rest spell it.
    ["hangman.bas", ["E", "O", "E", "I", "T", "N", "M", "R", "N"]],
    ["mastermind.bas", ["1234", "4323", "N"]],
    // 11 -> 19 -> 18, where the wumpus in 9 is one tunnel away.
    ["wumpus.bas", ["M", "19", "M", "18", "S", "9", "N"]],
    // Ten years of buy nothing, sell what the granary is short of, feed
    // everyone their twenty bushels, and plant every acre the people can work.
    ["hammurabi.bas", ["0", "16", "2000", "984", "0", "0", "2020", "984",
                       "0", "62", "2040", "922", "0", "0", "2100", "922",
                       "0", "5", "2160", "917", "0", "99", "2200", "818",
                       "0", "65", "2260", "753", "0", "114", "2300", "639",
                       "0", "72", "2360", "567", "0", "134", "2460", "433"]],
    ["eliza.bas", ["I AM SAD", "MY MOTHER IS NICE", "WHY", "BYE"]],
];

for (const [file, answers] of CASES) {
    put("/tmp/x.bas", readFileSync(join(HERE, "../examples", file), "utf8"));
    const out = body(session(['LOAD "/tmp/x.bas"', "RUN", ...answers]));
    // ?REDO FROM START and ?EXTRA IGNORED are INPUT's two complaints and carry
    // no "ERROR": they mean the answers stopped lining up with the prompts,
    // which is exactly the mistake this table is easy to make.
    for (const bad of ["ERROR", "REDO FROM START", "EXTRA IGNORED"])
        if (out.includes(bad))
            die(`${file} printed ${bad}:\n${out}`);
    // A run that fell off the end of its answers stops mid-prompt instead of
    // returning to the interpreter.
    if (!out.endsWith("\r\nOK\r\n"))
        die(`${file} did not run to completion:\n${out}`);
    golden(`examples/${file.replace(/\.bas$/, ".log")}`, out);
}

// The store fallback (epath.cpp): a bare name the working directory does not
// hold is looked for among the examples the package ships. The scan matches the
// `mbasic-' prefix, so the version here is arbitrary. The directories have to
// be planted too -- FakeStore keeps them in a set of their own, and store.files
// alone leaves list_dir with nothing to find.
const STORE = "/pkg/store/mbasic-9.9-r9";
for (const d of ["/pkg", "/pkg/store", STORE, `${STORE}/share`])
    H.store.dirs.add(d);
put(`${STORE}/share/hello.bas`, readFileSync(join(HERE, "../examples/hello.bas"), "utf8"));
const bare = body(session(['LOAD "hello.bas"', "RUN"]));
if (!bare.includes("HELLO, WORLD!"))
    die(`LOAD of a bare name did not reach the package's share:\n${bare}`);

ok();
