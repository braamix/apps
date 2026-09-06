10 rem Math functions demo
20 rem Demonstrates built-in math functions
30 print "Math functions demo"
40 print "==================="
50 print
60 input "Enter a number"; x
70 print
80 print "abs("; x; ") ="; abs(x)
90 print "int("; x; ") ="; int(x)
100 print "sgn("; x; ") ="; sgn(x)
110 if x >= 0 then print "sqr("; x; ") ="; sqr(x)
120 print
130 print "Trigonometry (radians):"
140 print "sin("; x; ") ="; sin(x)
150 print "cos("; x; ") ="; cos(x)
160 print "tan("; x; ") ="; tan(x)
170 print "atn("; x; ") ="; atn(x)
180 print
190 if x > 0 then print "log("; x; ") ="; log(x)
200 print "exp("; x; ") ="; exp(x)
210 print
220 input "Another (y/n)"; a$
230 if a$ = "Y" or a$ = "y" then 50
240 end
