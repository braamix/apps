short: libl.a(short.o)
	echo short
long: libl.a(a-rather-long-member.o)
	echo long $?
