10 rem Temperature converter
20 rem Converts between celsius and fahrenheit
30 def fn c2f(c) = c * 9 / 5 + 32
40 def fn f2c(f) = (f - 32) * 5 / 9
50 print "Temperature converter"
60 print "====================="
70 print
80 print "1. Celsius to Fahrenheit"
90 print "2. Fahrenheit to Celsius"
100 print "3. Exit"
110 print
120 input "Choice"; ch
130 if ch = 3 then 220
140 if ch < 1 or ch > 3 then 70
150 print
160 if ch = 1 then input "Enter Celsius"; t : print t; "C ="; fn c2f(t); "F"
170 if ch = 2 then input "Enter Fahrenheit"; t : print t; "F ="; fn f2c(t); "C"
180 print
190 input "Another conversion (y/n)"; a$
200 if a$ = "Y" or a$ = "y" then 70
210 print
220 print "Goodbye!"
230 end
