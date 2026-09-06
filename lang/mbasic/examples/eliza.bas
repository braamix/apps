10 rem ================================
20 rem Eliza - computer therapist
30 rem Based on weizenbaum's 1966 ai
40 rem ================================
50 print "Eliza - computer therapist"
60 print "Based on Weizenbaum (1966)"
70 print
80 print "Hello, I am Eliza."
90 print "Please tell me your problem."
100 print
110 rem Main loop
120 input "You: ";a$
130 if a$ = "" then 120
140 if a$ = "BYE" or a$ = "bye" then 900
150 if a$ = "QUIT" or a$ = "quit" then 900
160 rem Convert to uppercase
170 u$ = ""
180 for i = 1 to len(a$)
190 c$ = mid$(a$,i,1)
200 c = asc(c$)
210 if c >= 97 and c <= 122 then c = c - 32
220 u$ = u$ + chr$(c)
230 next i
240 a$ = u$
250 rem Check for keywords using gosub
260 rem Family words
270 k$ = "MOTHER": gosub 950: if f = 1 then 500
280 k$ = "FATHER": gosub 950: if f = 1 then 510
290 k$ = "SISTER": gosub 950: if f = 1 then 520
300 k$ = "BROTHER": gosub 950: if f = 1 then 520
310 k$ = "FAMILY": gosub 950: if f = 1 then 530
320 rem Feeling words
330 k$ = "SAD": gosub 950: if f = 1 then 540
340 k$ = "HAPPY": gosub 950: if f = 1 then 550
350 k$ = "ANGRY": gosub 950: if f = 1 then 560
360 k$ = "AFRAID": gosub 950: if f = 1 then 570
370 k$ = "DEPRESSED": gosub 950: if f = 1 then 580
380 rem Questions
390 k$ = "WHY": gosub 950: if f = 1 then 590
400 k$ = "HOW": gosub 950: if f = 1 then 600
410 k$ = "WHAT": gosub 950: if f = 1 then 610
420 k$ = "WHO": gosub 950: if f = 1 then 620
430 rem I statements
440 k$ = "I AM ": gosub 950: if f = 1 then 630
450 k$ = "I FEEL ": gosub 950: if f = 1 then 640
460 k$ = "I WANT ": gosub 950: if f = 1 then 650
470 k$ = "I NEED ": gosub 950: if f = 1 then 660
480 k$ = "I THINK ": gosub 950: if f = 1 then 670
490 goto 700
500 print "Eliza: Tell me more about your mother.": goto 120
510 print "Eliza: How do you feel about your father?": goto 120
520 print "Eliza: Tell me about your family.": goto 120
530 print "Eliza: Family is important. go on.": goto 120
540 print "Eliza: I am sorry to hear you are sad.": goto 120
550 print "Eliza: What makes you happy?": goto 120
560 print "Eliza: Why does that make you angry?": goto 120
570 print "Eliza: What are you afraid of?": goto 120
580 print "Eliza: Why do you feel depressed?": goto 120
590 print "Eliza: Why do you ask?": goto 120
600 print "Eliza: Does that question interest you?": goto 120
610 print "Eliza: Why do you ask that?": goto 120
620 print "Eliza: Who do you think?": goto 120
630 k$ = "I AM ": gosub 950
635 print "Eliza: Why do you say you are ";
636 print mid$(a$,p+5); "?": goto 120
640 k$ = "I FEEL ": gosub 950
645 print "Eliza: Do you often feel ";
646 print mid$(a$,p+7); "?": goto 120
650 k$ = "I WANT ": gosub 950
655 print "Eliza: Why do you want ";
656 print mid$(a$,p+7); "?": goto 120
660 k$ = "I NEED ": gosub 950
665 print "Eliza: Do you really need ";
666 print mid$(a$,p+7); "?": goto 120
670 k$ = "I THINK ": gosub 950
675 print "Eliza: Why do you think ";
676 print mid$(a$,p+8); "?": goto 120
700 rem Generic responses
710 r = int(rnd(1)*8) + 1
720 on r goto 730,740,750,760,770,780,790,795
730 print "Eliza: Please go on.": goto 120
740 print "Eliza: Tell me more.": goto 120
750 print "Eliza: That is interesting.": goto 120
760 print "Eliza: How does that make you feel?": goto 120
770 print "Eliza: Can you elaborate?": goto 120
780 print "Eliza: I see.": goto 120
790 print "Eliza: Please continue.": goto 120
795 print "Eliza: Very interesting.": goto 120
900 print
910 print "Eliza: Goodbye. it was nice"
920 print "       Talking with you."
930 end
940 rem
950 rem Find k$ in a$, set f=1 if found, p=position
960 f = 0: p = 0
970 l = len(k$)
980 if l > len(a$) then return
990 for j = 1 to len(a$) - l + 1
1000 if mid$(a$,j,l) = k$ then f = 1: p = j: return
1010 next j
1020 return
