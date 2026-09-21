// The tails are the matrix colour and a head is white.
//
// A Braam cell has sixteen colours. Default is green; `!` paints red.

import { boot, die, fg_at, ok, press, row, start, tick } from "./cmlib.mjs";

await boot("colour");
start();
tick(20);

const GREEN = 2, WHITE = 7, RED = 1;

let green = 0, white = 0;
for (let y = 0; y < 24; y++) {
    const line = row(y);
    for (let x = 0; x < line.length; x += 2) {
        if (!line[x] || line[x] === " ") continue;
        const c = fg_at(y, x);
        if (c === GREEN) green++;
        if (c === WHITE) white++;
    }
}
if (green === 0) die("no green tails");
if (white === 0) die("no white heads");

press("!");
tick(4);
let red = 0;
for (let y = 0; y < 24; y++) {
    const line = row(y);
    for (let x = 0; x < line.length; x += 2) {
        if (!line[x] || line[x] === " ") continue;
        if (fg_at(y, x) === RED) red++;
    }
}
if (red === 0) die("! did not paint red");

ok(`green tails, white heads, ! red (${green} green, ${white} white, ${red} red)`);
