.DEFAULT_GOAL = second
first:
	echo first [$(MAKECMDGOALS)]
second:
	echo second [$(MAKECMDGOALS)] [$(.DEFAULT_GOAL)]
