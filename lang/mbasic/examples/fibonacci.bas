10 rem Fibonacci sequence generator
20 rem Displays the first 20 fibonacci numbers
30 print "Fibonacci sequence"
40 print "=================="
50 a = 0 : b = 1
60 for i = 1 to 20
70   print a;
80   c = a + b
90   a = b : b = c
100 next i
110 print
120 end
