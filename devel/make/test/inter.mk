prog: mid
	echo link > $@
mid: src
	echo mid > $@
.INTERMEDIATE: mid
