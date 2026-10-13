all: keep1 keep2
keep1 keep2: src
	echo new >$@; sleep 100
