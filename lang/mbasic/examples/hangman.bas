10 rem Hangman - word guessing game
20 rem Guess the word before you're hanged!
30 dim w$(10) : rem WORD LIST
40 w$(1) = "APPLE" : w$(2) = "BASIC" : w$(3) = "COMPUTER"
50 w$(4) = "PROGRAM" : w$(5) = "KEYBOARD" : w$(6) = "MONITOR"
60 w$(7) = "FLOPPY" : w$(8) = "WOZNIAK" : w$(9) = "INTEGER"
70 w$(10) = "MICROSOFT"
80 print "Hangman"
90 print "======="
100 print
110 rem Pick a word
120 w$ = w$(int(rnd(1) * 10) + 1)
130 l = len(w$)
140 g$ = "" : rem GUESSED LETTERS
150 bad = 0
160 rem Build display string
170 d$ = ""
180 for i = 1 to l
190   c$ = mid$(w$, i, 1)
200   found = 0
210   for j = 1 to len(g$)
220     if mid$(g$, j, 1) = c$ then found = 1
230   next j
240   if found = 1 then d$ = d$ + c$ + " "
245   if found = 0 then d$ = d$ + "_ "
250 next i
260 rem Display
270 print
280 print "Word: "; d$
290 print "Wrong guesses:"; bad; "/ 6"
300 if len(g$) > 0 then print "Guessed: "; g$
310 rem Check win
320 win = 1
330 for i = 1 to l
340   c$ = mid$(w$, i, 1)
350   found = 0
360   for j = 1 to len(g$)
370     if mid$(g$, j, 1) = c$ then found = 1
380   next j
390   if found = 0 then win = 0
400 next i
410 if win = 1 then print : print "You win! The word was "; w$ : goto 600
420 if bad >= 6 then print : print "Hanged! The word was "; w$ : goto 600
430 rem Get guess
440 input "Guess a letter"; c$
450 c$ = left$(c$, 1)
455 if c$ >= "a" and c$ <= "z" then c$ = chr$(asc(c$) - 32)
460 rem Check if already guessed
470 for i = 1 to len(g$)
480   if mid$(g$, i, 1) = c$ then print "Already guessed!" : goto 440
490 next i
500 g$ = g$ + c$
510 rem Check if in word
520 found = 0
530 for i = 1 to l
540   if mid$(w$, i, 1) = c$ then found = 1
550 next i
560 if found = 0 then bad = bad + 1 : print "Wrong!"
570 goto 160
600 print
610 input "Play again (y/n)"; a$
620 if a$ = "Y" or a$ = "y" then 110
630 end
