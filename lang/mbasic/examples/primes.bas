10 rem Prime number finder
20 rem Finds all primes up to a given limit
30 print "Prime number finder"
40 print "==================="
50 print
60 input "Find primes up to"; limit
70 print
80 print "Primes:";
90 count = 0
100 for n = 2 to limit
110   isprime = 1
115   rem A for body always runs once, so 2 and 3 must not reach the division
117   if n < 4 then 150
120   for i = 2 to sqr(n)
130     if n / i = int(n / i) then isprime = 0 : i = n
140   next i
150   if isprime = 1 then print n; : count = count + 1
160 next n
170 print
180 print
190 print "Found"; count; "primes"
200 end
