10 rem Factorial calculator
20 rem Calculates n! using gosub
30 print "Factorial calculator"
40 print "===================="
50 print
60 input "Enter a number (0-12)"; n
70 if n < 0 or n > 12 then print "Out of range!" : goto 50
80 gosub 200
90 print n; "! ="; f
100 print
110 input "Another (y/n)"; a$
120 if a$ = "Y" or a$ = "y" then 50
130 end
200 rem Factorial subroutine
210 rem Input: n, output: f
220 f = 1
230 for i = 1 to n
240   f = f * i
250 next i
260 return
