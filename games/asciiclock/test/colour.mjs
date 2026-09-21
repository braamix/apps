import { boot, die, fg_at, bg_at, ok, start, tick } from "./acl.mjs";

await boot("colour");
start();
tick(20);

let fg = 0, bg = 0;
for (let y = 0; y < 24; y++) {
    for (let x = 0; x < 80; x++) {
        if (fg_at(y, x) !== 0) fg++;
        if (bg_at(y, x) !== 0) bg++;
    }
}
if (fg === 0) die("no non-black foreground");
if (bg === 0) die("no non-black background");

ok(`colour on screen (${fg} fg cells, ${bg} bg cells)`);
