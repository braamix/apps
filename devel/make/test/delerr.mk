.DELETE_ON_ERROR:
out: src
	echo partial > $@
	false
old: src
	false
