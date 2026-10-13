all: a b c
	echo all
a:
	echo a1
	false
	echo a3
b:
	echo b1
	echo b2
c:
	echo c
