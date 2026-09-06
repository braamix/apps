10 rem Calendar - print a monthly calendar
20 rem Classic utility program
25 dim m$(12), d(12) : rem DIMENSIONED ONCE, AHEAD OF THE REPLAY LOOP
30 print "Calendar generator"
40 print "=================="
50 print
60 input "Year"; yr
70 input "Month (1-12)"; mo
80 if mo < 1 or mo > 12 then 70
90 rem Month names
110 m$(1) = "January" : m$(2) = "February" : m$(3) = "March"
120 m$(4) = "April" : m$(5) = "May" : m$(6) = "June"
130 m$(7) = "July" : m$(8) = "August" : m$(9) = "September"
140 m$(10) = "October" : m$(11) = "November" : m$(12) = "December"
150 rem Days in each month
160 d(1) = 31 : d(2) = 28 : d(3) = 31 : d(4) = 30
170 d(5) = 31 : d(6) = 30 : d(7) = 31 : d(8) = 31
180 d(9) = 30 : d(10) = 31 : d(11) = 30 : d(12) = 31
190 rem Leap year check
200 leap = 0
210 if yr / 4 = int(yr / 4) then leap = 1
220 if yr / 100 = int(yr / 100) then leap = 0
230 if yr / 400 = int(yr / 400) then leap = 1
240 if leap = 1 then d(2) = 29
250 rem Zeller: day of week of the first, 0 is saturday
260 y = yr : m = mo
270 if m < 3 then m = m + 12 : y = y - 1
280 k = y - int(y / 100) * 100
290 j = int(y / 100)
300 f = 1 + int(13 * (m + 1) / 5) + k + int(k / 4) + int(j / 4) - 2 * j
310 rem Shift to sunday-first. int is floor, so this is a modulo
320 dow = f + 6 : dow = dow - int(dow / 7) * 7
330 rem Title, centred over the twenty columns of a week
340 t$ = m$(mo) + str$(yr)
350 print
360 print spc(int((20 - len(t$)) / 2) + 1); t$
370 print " Su Mo Tu We Th Fr Sa"
375 print "---------------------"
380 rem Leading blanks. Not a for: its body always runs once
390 w$ = " " : i = dow
400 if i <= 0 then 430
410 w$ = w$ + "   " : i = i - 1
420 goto 400
430 rem str$ right-aligns a day in two columns
440 for day = 1 to d(mo)
450   w$ = w$ + right$(str$(day), 2) + " "
460   dow = dow + 1
470   if dow = 7 then gosub 600 : dow = 0
480 next day
490 if dow > 0 then gosub 600
500 print
510 input "Another month (y/n)"; a$
520 if a$ = "Y" or a$ = "y" then 50
530 end
600 rem Print the week, less its trailing blank
610 print left$(w$, len(w$) - 1)
620 w$ = " "
630 return
