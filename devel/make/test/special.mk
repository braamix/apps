.POSIX:
.NOTPARALLEL:
.SCCS_GET:
	sccs $(SCCSFLAGS) get $@
all: one .WAIT two
	echo all after $^
one two:
	echo $@
