10 rem Mastermind - code breaking game
20 rem Guess the secret 4-digit code
25 dim s(4), g(4), u(4), v(4) : rem DIMENSIONED ONCE, AHEAD OF THE LOOPS
30 print "Mastermind"
40 print "=========="
50 print
60 print "I'm thinking of a 4-digit code (1-6)"
70 print "After each guess I'll tell you:"
80 print "  Black = right number, right place"
90 print "  White = right number, wrong place"
100 print
110 rem Generate secret code
130 for i = 1 to 4
140   s(i) = int(rnd(1) * 6) + 1
150 next i
160 tries = 0
170 rem Get guess
180 print
190 input "Your guess (4 digits, e.g. 1234)"; g$
200 if len(g$) <> 4 then print "Enter 4 digits!" : goto 190
210 rem Parse guess
220 for i = 1 to 4
230   g(i) = val(mid$(g$, i, 1))
240   if g(i) < 1 or g(i) > 6 then print "Use digits 1-6!" : goto 190
250 next i
260 tries = tries + 1
270 rem Count black (exact matches)
280 black = 0
290 rem Used flags
300 for i = 1 to 4 : u(i) = 0 : v(i) = 0 : next i
310 for i = 1 to 4
320   if g(i) = s(i) then black = black + 1 : u(i) = 1 : v(i) = 1
330 next i
340 rem Count white (wrong place)
350 white = 0
360 for i = 1 to 4
370   if u(i) = 1 then 420
380   for j = 1 to 4
390     if v(j) = 1 then 410
400     if g(i) = s(j) then white = white + 1 : v(j) = 1 : j = 4
410   next j
420 next i
430 rem Display result
440 print "Black:"; black; " White:"; white
450 if black = 4 then 500
460 if tries >= 10 then 550
470 goto 170
500 print
510 print "You cracked the code in"; tries; "tries!"
520 goto 600
550 print
560 print "Out of tries! The code was:";
570 for i = 1 to 4 : print s(i); : next i
580 print
600 print
610 input "Play again (y/n)"; a$
620 if a$ = "Y" or a$ = "y" then 110
630 end
