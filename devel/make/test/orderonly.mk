prog: a.o | objdir
	echo link $@
a.o: | objdir
	echo cc $@
objdir:
	echo mkdir $@
