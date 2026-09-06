10 rem Bubble sort demonstration
20 rem Sorts an array of random numbers
30 print "Bubble sort demo"
40 print "================"
50 print
60 n = 10
70 dim a(10)
80 rem Fill array with random numbers
90 print "Original array:"
100 for i = 1 to n
110   a(i) = int(rnd(1) * 100)
120   print a(i);
130 next i
140 print
150 print
160 rem Bubble sort
170 for i = 1 to n - 1
180   for j = 1 to n - i
190     if a(j) > a(j + 1) then t = a(j) : a(j) = a(j + 1) : a(j + 1) = t
200   next j
210 next i
220 rem Display sorted array
230 print "Sorted array:"
240 for i = 1 to n
250   print a(i);
260 next i
270 print
280 end
