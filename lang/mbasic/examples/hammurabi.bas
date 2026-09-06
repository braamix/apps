10 rem Hammurabi - classic resource management game
20 rem Rule the ancient city of Sumeria
30 print "Hammurabi"
40 print "========="
50 print
60 print "Try your hand at governing ancient Sumeria"
70 print "For 10 years. Good luck!"
80 print
90 rem Initialize
100 year = 0
110 pop = 100 : rem POPULATION
120 acres = 1000 : rem ACRES OF LAND
130 grain = 2800 : rem BUSHELS IN STORE
140 harvest = 3 : rem BUSHELS PER ACRE
150 starved = 0 : rem STARVED THIS YEAR
160 ill = 0
170 rem Main game loop
180 year = year + 1
190 if year > 10 then 700
200 print
210 print "Year"; year; "of your reign"
220 print "Population:"; pop
230 print "Acres:"; acres
240 print "Grain in store:"; grain; "bushels"
250 print
260 rem Random land price
270 price = int(rnd(1) * 10) + 17
280 print "Land is trading at"; price; "bushels/acre"
290 rem Buy land
300 input "Acres to buy"; buy
310 if buy < 0 then 300
320 if buy * price > grain then print "Not enough grain!" : goto 300
330 acres = acres + buy
340 grain = grain - buy * price
350 if buy > 0 then 420
360 rem Sell land
370 input "Acres to sell"; sell
380 if sell < 0 then 370
390 if sell > acres then print "Not enough land!" : goto 370
400 acres = acres - sell
410 grain = grain + sell * price
420 rem Feed people
430 print "Grain in store:"; grain
440 input "Bushels to feed people"; feed
450 if feed < 0 then 440
460 if feed > grain then print "Not enough grain!" : goto 440
470 grain = grain - feed
480 rem Plant crops
490 print "Grain in store:"; grain
500 input "Acres to plant"; plant
510 if plant < 0 then 500
520 if plant > acres then print "Not enough land!" : goto 500
530 if plant > grain * 2 then print "Not enough seed!" : goto 500
540 if plant > pop * 10 then print "Not enough people!" : goto 500
550 grain = grain - int(plant / 2)
560 rem Calculate harvest
570 harvest = int(rnd(1) * 6) + 1
580 grain = grain + harvest * plant
590 rem Rats
600 if rnd(1) > 0.4 then 630
610 rats = int(grain * rnd(1) * 0.2)
620 grain = grain - rats : print "Rats ate"; rats; "bushels!"
630 rem Starvation
640 ate = int(feed / 20)
650 if ate >= pop then starved = 0 : goto 670
660 starved = pop - ate : pop = ate
670 rem Births
680 births = int(rnd(1) * 6) + 1
690 pop = pop + births
695 goto 180
700 rem End of game
710 print
720 print "Your reign has ended!"
730 print "Final population:"; pop
740 print "Acres per person:"; int(acres / pop)
750 if pop < 50 then print "Terrible! The people revolted!"
760 if pop >= 50 and pop < 100 then print "Mediocre rule."
770 if pop >= 100 then print "Excellent! Hail Hammurabi!"
780 end
