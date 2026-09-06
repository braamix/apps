10 rem A random 8x8 maze, drawn with box characters
20 rem
30 rem The sixteen glyphs sit in one string and are picked by a bitmask of the
40 rem walls that touch a square: 1 up, 2 right, 4 down, 8 left. mid$ counts
50 rem characters, so mid$(b$, m + 1, 1) is the glyph for mask m even though
60 rem every one of them is three bytes long.
70 b$ = " ╵╶└╷│┌├╴┘─┴┐┤┬┼"
80 print "A random maze. Each glyph is one character of"; len(b$); "in b$."
90 print
100 n = 8 : rem cells across and down
110 g = 2 * n + 1 : rem the picture is g squares each way
120 rem
130 rem w is the wall grid, with a ring of empty squares round it so that
140 rem looking at a neighbour never runs off the end. q is the same trick for
150 rem the cells: the ring counts as visited, so it is never dug into.
160 dim w(g + 1, g + 1), q(n + 1, n + 1), sx(n * n), sy(n * n), dx(4), dy(4)
170 for i = 1 to g : for j = 1 to g : w(i, j) = 1 : next j : next i
180 for i = 0 to n + 1
190 q(i, 0) = 1 : q(i, n + 1) = 1 : q(0, i) = 1 : q(n + 1, i) = 1
200 next i
210 rem
220 rem Cell (cx, cy) runs 1 to n and sits at w(2 * cx, 2 * cy); the wall
230 rem between two cells is the square between them.
240 cx = 1 : cy = 1 : q(cx, cy) = 1 : w(2, 2) = 0 : sp = 0
250 rem
260 rem Dig: walk to a random unvisited neighbour, and when there is none, back
270 rem up the stack. This is the recursive backtracker, with the recursion
280 rem written out, since there is none to be had here.
290 nc = 0
300 if q(cx - 1, cy) = 0 then nc = nc + 1 : dx(nc) = -1 : dy(nc) = 0
310 if q(cx + 1, cy) = 0 then nc = nc + 1 : dx(nc) = 1 : dy(nc) = 0
320 if q(cx, cy - 1) = 0 then nc = nc + 1 : dx(nc) = 0 : dy(nc) = -1
330 if q(cx, cy + 1) = 0 then nc = nc + 1 : dx(nc) = 0 : dy(nc) = 1
340 if nc = 0 then goto 430
350 k = int(rnd(1) * nc) + 1
360 nx = cx + dx(k) : ny = cy + dy(k)
370 w(2 * cx + dx(k), 2 * cy + dy(k)) = 0
380 w(2 * nx, 2 * ny) = 0
390 q(nx, ny) = 1
400 sp = sp + 1 : sx(sp) = cx : sy(sp) = cy
410 cx = nx : cy = ny
420 goto 290
430 if sp = 0 then goto 460
440 cx = sx(sp) : cy = sy(sp) : sp = sp - 1
450 goto 290
460 rem A way in at the top left and out at the bottom right
470 w(2, 1) = 0 : w(2 * n, g) = 0
480 rem
490 rem Draw. A square that is not a wall is a space, which is glyph 0, so the
500 rem mask is left at zero for it.
510 rem
520 rem One square to a cell reads far too narrow for a corridor, so the even
530 rem columns -- the ones a cell sits in, rather than a wall -- go out three
540 rem times. Only a bar or a space is ever there, and both repeat cleanly.
550 for j = 1 to g
560 l$ = ""
570 for i = 1 to g
580 m = 0
590 if w(i, j) = 0 then goto 640
600 if w(i, j - 1) = 1 then m = m + 1
610 if w(i + 1, j) = 1 then m = m + 2
620 if w(i, j + 1) = 1 then m = m + 4
630 if w(i - 1, j) = 1 then m = m + 8
640 c$ = mid$(b$, m + 1, 1)
650 l$ = l$ + c$
660 if i = 2 * int(i / 2) then l$ = l$ + c$ + c$
670 next i
680 print l$
690 next j
700 end
