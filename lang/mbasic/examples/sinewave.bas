10 rem Sine wave - Apple ii graphics in text
20 rem Draws a sine wave using characters
30 print "Sine wave display"
40 print "================="
50 print
60 for y = 0 to 50
70   x = int(20 + sin(y / 5) * 18)
80   for i = 1 to x
90     print " ";
100   next i
110   print "*"
120 next y
130 print
140 input "Again (y/n)"; a$
150 if a$ = "Y" or a$ = "y" then 50
160 end
