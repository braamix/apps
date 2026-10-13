bad:
	test 1 -eq 2
	echo never
ign:
	-test 1 -eq 2
	echo carried on
