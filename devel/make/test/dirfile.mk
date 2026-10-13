.POSIX:
all: sub/dir/x.o y.o
	echo [$(@D)] [$(@F)] [$(?D)] [$(?F)] [$(^D)] [$(^F)] [${<D}] [${<F}]
sub/dir/x.o y.o:
	echo [$(@D)] [$(@F)] [$(*D)] [$(*F)]
