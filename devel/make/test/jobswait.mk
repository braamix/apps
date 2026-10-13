all: a b .WAIT c
	echo all
a b c:
	echo $@ one
	echo $@ two
