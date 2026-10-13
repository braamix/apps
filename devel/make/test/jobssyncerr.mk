all: a b
a:
	echo a1; echo a-err >&2
	false
b:
	echo b1
	echo b2
