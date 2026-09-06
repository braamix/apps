10 rem String functions demo
20 rem Demonstrates built-in string functions
30 print "String functions demo"
40 print "====================="
50 print
60 input "Enter a string"; s$
70 print
80 print "Your string: '"; s$; "'"
90 print "Length: "; len(s$)
100 print
110 if len(s$) > 0 then print "First char: '"; left$(s$, 1); "' ASCII:"; asc(s$)
120 if len(s$) > 0 then print "Last char:  '"; right$(s$, 1); "'"
130 if len(s$) >= 3 then print "Middle 3:   '"; mid$(s$, int(len(s$)/2), 3); "'"
140 print
150 print "left$(s$,3):  '"; left$(s$, 3); "'"
160 print "right$(s$,3): '"; right$(s$, 3); "'"
170 print "mid$(s$,2,4): '"; mid$(s$, 2, 4); "'"
180 print
190 print "chr$(65) = '"; chr$(65); "'"
200 print "str$(42) = '"; str$(42); "'"
210 print "val("; chr$(34); "123"; chr$(34); ") ="; val("123")
220 print
230 input "Another (y/n)"; a$
240 if a$ = "Y" or a$ = "y" then 50
250 end
