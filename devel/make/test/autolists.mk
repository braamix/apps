prog: b.o a.o b.o | d1 d2 d1
	echo [$^] [$+] [$|] [$<] [$?]
b.o a.o d1 d2:
	echo $@
