10 rem Slot machine - Apple ii classic
20 rem Try your luck at the slots!
30 dim s$(6)
40 s$(1) = "Cherry" : s$(2) = "Lemon" : s$(3) = "Orange"
50 s$(4) = "Plum" : s$(5) = "Bell" : s$(6) = "Seven"
60 print "Slot machine"
70 print "============"
80 print
90 print "Match 2 = 2x bet"
100 print "Match 3 = 10x bet"
110 print "Three 7s = 100x bet!"
120 print
130 cash = 100
140 print "You have $"; cash
150 if cash <= 0 then print "You're broke!" : goto 400
160 input "Bet (0 to quit)"; bet
170 if bet = 0 then 400
180 if bet > cash then print "Not enough cash!" : goto 160
190 if bet < 1 then 160
200 cash = cash - bet
210 rem Spin the wheels
220 print
230 r1 = int(rnd(1) * 6) + 1
240 r2 = int(rnd(1) * 6) + 1
250 r3 = int(rnd(1) * 6) + 1
260 print "[ "; s$(r1); " ]  [ "; s$(r2); " ]  [ "; s$(r3); " ]"
270 print
280 rem Check for wins
290 if r1 = 6 and r2 = 6 and r3 = 6 then win = bet * 100 : print "Jackpot!!!" : goto 350
300 if r1 = r2 and r2 = r3 then win = bet * 10 : print "Three of a kind!" : goto 350
310 if r1 = r2 or r2 = r3 or r1 = r3 then win = bet * 2 : print "Two match!" : goto 350
320 print "No match"
330 goto 140
350 print "You win $"; win
360 cash = cash + win
370 goto 140
400 print
410 print "Thanks for playing!"
420 print "Final total: $"; cash
430 end
