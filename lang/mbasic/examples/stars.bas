10 rem Star pattern generator
20 rem Creates a triangle of stars
30 print "Star triangle"
40 print "============="
50 print
60 input "Number of rows"; r
70 print
80 for i = 1 to r
90   rem Print leading spaces
100   for j = 1 to r - i
110     print " ";
120   next j
130   rem Print stars
140   for j = 1 to 2 * i - 1
150     print "*";
160   next j
170   print
180 next i
190 end
