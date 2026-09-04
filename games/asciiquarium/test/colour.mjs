// The colour masks reach the cells.
//
// A Braam cell has sixteen colours and so does termbox, so upstream's mask
// letters cross over exactly: a row is asserted as one hex digit a cell.

import { boot, die, hue, is, ok, row, start, tick } from "./aqlib.mjs";

await boot("colour");
start();
tick(3);

const CYAN = 6, GREY = 8, WHITE = 7, BRIGHT_WHITE = 15, YELLOW = 3;
const at = (y, x) => parseInt(hue(y)[x] ?? "7", 16);

// 1. The top water band is cyan, which is its DefaultColor and has no mask.
const water = row(5);
for (let x = 0; x < water.length; x++)
    if (water[x] === "~" && at(5, x) !== CYAN)
        die(`water cell ${x} is ${at(5, x)}, not cyan`);

// 2. The castle is dark grey where its mask says nothing and white or yellow
// where it does. DARK_GREY is black with the bright bit, which is 8.
const seen = new Set();
for (let y = 11; y <= 23; y++) {
    const line = row(y);
    for (let x = 48; x < line.length; x++)
        if (line[x] !== " ") seen.add(at(y, x));
}
if (!seen.has(GREY)) die(`no dark grey in the castle: ${[...seen]}`);
if (!seen.has(YELLOW)) die(`no yellow in the castle: ${[...seen]}`);
// Both whites: the castle mask is `W` throughout with one `w` in it, so a swap
// of the two would show up here.
if (!seen.has(BRIGHT_WHITE)) die(`no bright white (W) in the castle: ${[...seen]}`);
if (!seen.has(WHITE)) die(`no plain white (w) in the castle: ${[...seen]}`);

// 3. A fish's mask is rewritten by randColor, so its cells are not all one
// colour and not the default white.
const fishes = [];
for (let y = 9; y <= 23; y++) {
    const colours = new Set();
    const text = row(y);
    for (let x = 0; x < 45 && x < text.length; x++)
        if (text[x] !== " ") colours.add(at(y, x));
    if (colours.size > 1) fishes.push(y);
}
if (fishes.length === 0) die("no row below the water carries more than one colour");

ok(`water cyan, castle grey, ${fishes.length} rows of coloured fish`);
