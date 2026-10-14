H = $(firstword $(wildcard hdr*.h))
S = $(shell echo shell sees [$(H)] >&2; echo got $(H))
all:
	@echo [$(S)]
