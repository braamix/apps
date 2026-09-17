// The three demos under examples/, run as a user would run them.
//
// Each is planted where the shell's cwd is, so `py hello.py` finds it the way
// `python hello.py` finds one in a directory of its own. guess.py takes its
// number from GUESS_SEED, which is what makes a played game assertable --
// adventure pins ADVENTURE_SEED for the same reason.

import { readFileSync, readdirSync } from "node:fs";
import { join } from "node:path";

import { boot, put, run, ok, die, same, APPS } from "./pylib.mjs";

await boot("pyexamples");
let bad = 0;
const check = (what, got, want) => {
    if (!same(what, got, want)) bad++;
};

const DIR = join(APPS, "lang/python/examples");
const names = readdirSync(DIR).filter((f) => f.endsWith(".py")).sort();
if (names.length !== 3) die(`examples/ holds ${names.length} programs, not three`);
for (const n of names) put(`/home/${n}`, readFileSync(join(DIR, n)));

// ---------------------------------------------------------------- hello.py

{
    const r = run("hello.py");
    check(
        "hello.py",
        r.out,
        "Hello, world!\n" +
            "This is braam 3.14.0.\n" +
            "1. the (3 letters)\n" +
            "2. quick (5 letters)\n" +
            "3. brown (5 letters)\n" +
            "4. fox (3 letters)\n"
    );
    check("hello.py stderr", r.err, "");
    check("hello.py status", String(r.status), "0");
}

// ------------------------------------------------------------- fizzbuzz.py

{
    const r = run("fizzbuzz.py");
    check(
        "fizzbuzz.py with no argument counts to twenty",
        r.out,
        "1 2 Fizz 4 Buzz Fizz 7 8 Fizz Buzz 11 Fizz 13 14 FizzBuzz 16 17 Fizz 19 Buzz\n"
    );
    check("fizzbuzz.py status", String(r.status), "0");
}

{
    const r = run("fizzbuzz.py 15");
    check(
        "the argument is the count",
        r.out,
        "1 2 Fizz 4 Buzz Fizz 7 8 Fizz Buzz 11 Fizz 13 14 FizzBuzz\n"
    );
}

{
    // A bad argument is a diagnostic on stderr and a status, not a traceback.
    const r = run("fizzbuzz.py x");
    check("a count that is not a number", r.out, "");
    check("names the argument", r.err, "fizzbuzz.py: x is not a number\n");
    check("and fails", String(r.status), "1");
}

// ---------------------------------------------------------------- guess.py

// GUESS_SEED=1 is 18, in this interpreter and in CPython both: the Mersenne
// Twister is the same generator, so the same seed is the same game.
const SEED = "GUESS_SEED=1";
const HEAD = "Number guessing game\n====================\n\nI'm thinking of a number between 1 and 100.\n";

{
    // A game won by halving: 50 high, 25 high, 12 low, 18 right.
    const r = run("guess.py", "50\n25\n12\n18\nn\n", SEED);
    check(
        "a game played to the end",
        r.out,
        HEAD +
            "Your guess? Too high!\n" +
            "Your guess? Too high!\n" +
            "Your guess? Too low!\n" +
            "Your guess? \n" +
            "Correct! You got it in 4 tries.\n" +
            "\nPlay again (y/n)? Thanks for playing!\n"
    );
    check("guess.py status", String(r.status), "0");
}

{
    // The same number twice, because the seed is the same and the second game
    // draws again from where the first left off -- so this one is not 18.
    const r = run("guess.py", "18\ny\n18\nn\n", SEED);
    if (!r.out.includes("Correct! You got it in 1 try.\n"))
        die(`the first guess did not win: ${JSON.stringify(r.out)}`);
    if (r.out.split("I'm thinking").length !== 3)
        die("`y` did not start a second game");
    if (r.out.includes("You got it in 1 try.\nCorrect"))
        die("the second game drew the same number");
}

{
    // Not a number is said so and costs no try; the end of input leaves.
    const r = run("guess.py", "twelve\n18\n", SEED);
    check(
        "a guess that is not a number",
        r.out,
        HEAD +
            "Your guess? 'twelve' is not a number.\n" +
            "Your guess? \n" +
            "Correct! You got it in 2 tries.\n" +
            "\nPlay again (y/n)? \nThanks for playing!\n"
    );
}

{
    // Nothing typed at all: the first prompt reads the end of input.
    const r = run("guess.py", "", SEED);
    check("the end of input at the first prompt", r.out, HEAD + "Your guess? \nThanks for playing!\n");
    check("status", String(r.status), "0");
}

if (bad) process.exit(1);
ok(`${names.length} demos: ${names.join(", ")}`);
