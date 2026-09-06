10 rem Number guessing game
20 rem Guess a random number between 1 and 100
30 print "Number guessing game"
40 print "===================="
50 print
60 n = int(rnd(1) * 100) + 1
70 t = 0
80 print "I'm thinking of a number between 1 and 100."
90 print
100 input "Your guess"; g
110 t = t + 1
120 if g < n then print "Too low!" : goto 100
130 if g > n then print "Too high!" : goto 100
140 print
150 print "Correct! You got it in"; t; "tries!"
160 print
170 input "Play again (y/n)"; a$
180 if a$ = "Y" or a$ = "y" then 50
190 print "Thanks for playing!"
200 end
