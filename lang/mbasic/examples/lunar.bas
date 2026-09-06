10 rem Lunar lander - classic Apple ii game
20 rem Land your spacecraft safely on the moon
30 print "Lunar lander"
40 print "============"
50 print
60 print "You are landing on the moon."
70 print "Control your descent by setting fuel burn rate (0-30)."
80 print "Land with velocity < 5 to survive!"
90 print
100 rem Initialize
110 h = 1000 : rem HEIGHT IN FEET
120 v = 50 : rem VELOCITY (FEET/SEC, POSITIVE = FALLING)
130 f = 250 : rem FUEL REMAINING
140 g = 5 : rem LUNAR GRAVITY
150 print "Height", "Velocity", "Fuel"
160 print "------", "--------", "----"
170 rem Main loop
180 print int(h), int(v), int(f)
190 if h <= 0 then 300
200 input "Fuel burn rate (0-30)"; b
210 if b < 0 then b = 0
220 if b > 30 then b = 30
230 if b > f then b = f
240 rem Update physics
250 v = v + g - b
260 h = h - v
270 f = f - b
280 goto 180
300 rem Landing
310 if v > 5 then 350
320 print
330 print "Perfect landing! Welcome to the moon!"
340 goto 400
350 print
360 print "Crash! Velocity was"; int(v); "ft/sec"
370 if v > 20 then print "No survivors."
380 if v <= 20 then print "Rescue team dispatched."
400 print
410 input "Play again (y/n)"; a$
420 if a$ = "Y" or a$ = "y" then 100
430 end
