// A resize: the geometry rides on the reply to next_key(), so it is the
// keyboard task that reshapes the grid and the frame loop that reflows.
//
// Size-dependent scenery is rebuilt and everything else is clamped into the
// new height, which is what upstream's reflowForResize does.

import { boot, die, H, ok, regrid, row, start, tick } from "./aqlib.mjs";

await boot("resize");
start();
tick(2);

// 1. Eighty columns: the top band is tiled the whole way across.
if ((row(5).match(/~/g) ?? []).length < 78) die(`no full band at 80: ${row(5)}`);
if (!row(11).includes("T~~")) die(`no castle turret at 80: ${row(11)}`);

// 2. Forty by fifteen, upstream's stated minimum. The band re-tiles to the new
// width and the castle follows the corner, its base on the last row.
regrid(40, 15);
tick();
const narrow = H.rows(H.screen()).map((s) => s.replace(/\s+$/, ""));
if (narrow[5].length > 40) die(`the band is wider than the screen: ${narrow[5]}`);
if ((narrow[5].match(/~/g) ?? []).length < 38) die(`no full band at 40: ${narrow[5]}`);
if ((narrow[14].match(/_/g) ?? []).length < 15) die(`no castle base on row 14: ${narrow[14]}`);

// 3. Nothing is left below the new height: the fish were clamped, not dropped
// off the end.
if (narrow.length !== 15) die(`the screen is ${narrow.length} rows`);

// 4. And back: the scenery is rebuilt at the wider size.
regrid(80, 24);
tick();
if ((row(5).match(/~/g) ?? []).length < 78) die(`no full band back at 80: ${row(5)}`);
if (!row(11).includes("T~~")) die(`no castle turret back at 80: ${row(11)}`);

// 5. Below upstream's stated minimum it gives up, and says so on the real
// stderr: the claims go back first, or the message would die with the screen.
regrid(39, 14);
tick(2);
const said = H.rows(H.screen()).join(" ").replace(/\s+/g, " ");
if (!said.includes("terminal too small: need at least 40x15, got 39x14"))
    die(`no complaint at 39x14: ${said}`);
if (!H.rows(H.screen()).some((l) => l.startsWith(H.prompt(1)))) die("no [1] prompt");

ok("re-tiled, re-cornered, clamped, and refused below 40x15");
