10 rem Hunt the wumpus - classic text adventure
20 rem Navigate a cave to hunt the wumpus
30 dim c(20, 3) : rem CAVE CONNECTIONS
40 rem Cave map (dodecahedron)
50 for i = 1 to 20 : read c(i, 1), c(i, 2), c(i, 3) : next i
60 data 2,5,8, 1,3,10, 2,4,12, 3,5,14, 1,4,6
70 data 5,7,15, 6,8,17, 1,7,9, 8,10,18, 2,9,11
80 data 10,12,19, 3,11,13, 12,14,20, 4,13,15, 6,14,16
90 data 15,17,20, 7,16,18, 9,17,19, 11,18,20, 13,16,19
100 print "Hunt the wumpus"
110 print "==============="
120 print
130 print "The wumpus lives in a cave of 20 rooms."
140 print "Each room has 3 tunnels to other rooms."
150 print "Hazards: bottomless pits, super bats, and the wumpus!"
160 print
170 rem Place things randomly
180 you = int(rnd(1) * 20) + 1
190 wump = int(rnd(1) * 20) + 1
200 if wump = you then 190
210 p1 = int(rnd(1) * 20) + 1
220 if p1 = you or p1 = wump then 210
230 p2 = int(rnd(1) * 20) + 1
240 if p2 = you or p2 = wump or p2 = p1 then 230
250 b1 = int(rnd(1) * 20) + 1
260 if b1 = you or b1 = wump or b1 = p1 or b1 = p2 then 250
270 b2 = int(rnd(1) * 20) + 1
280 if b2 = you or b2 = wump or b2 = p1 or b2 = p2 or b2 = b1 then 270
290 arrows = 5
300 rem Main loop
310 print
320 print "You are in room"; you
330 print "Tunnels lead to"; c(you, 1); c(you, 2); c(you, 3)
340 rem Check for hazards nearby
350 for i = 1 to 3
360   r = c(you, i)
370   if r = wump then print "I smell a wumpus!"
380   if r = p1 or r = p2 then print "I feel a draft!"
390   if r = b1 or r = b2 then print "Bats nearby!"
400 next i
410 print
420 input "Shoot or move (s/m)"; a$
430 if a$ = "S" or a$ = "s" then 500
440 if a$ = "M" or a$ = "m" then 600
450 goto 420
500 rem Shoot
510 input "Shoot into which room"; r
520 if r <> c(you, 1) and r <> c(you, 2) and r <> c(you, 3) then print "Can't shoot there!" : goto 510
530 arrows = arrows - 1
540 if r = wump then print "You got the wumpus!" : goto 900
550 print "Missed!"
560 rem Wumpus wakes up
570 if rnd(1) > 0.75 then wump = c(wump, int(rnd(1) * 3) + 1)
580 if wump = you then print "The wumpus got you!" : goto 900
590 if arrows = 0 then print "Out of arrows!" : goto 900
595 goto 300
600 rem Move
610 input "Move to which room"; r
620 if r <> c(you, 1) and r <> c(you, 2) and r <> c(you, 3) then print "Can't go there!" : goto 610
630 you = r
640 rem Check hazards
650 if you = wump then print "The wumpus got you!" : goto 900
660 if you = p1 or you = p2 then print "Fell in a pit!" : goto 900
670 if you = b1 or you = b2 then print "Super bat grabbed you!" : you = int(rnd(1) * 20) + 1 : goto 640
680 goto 300
900 print
910 input "Play again (y/n)"; a$
920 if a$ = "Y" or a$ = "y" then 170
930 end
