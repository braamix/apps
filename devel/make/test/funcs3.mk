X := $(file >out.txt,hello)$(file >>out.txt,more)
R := $(file <out.txt)
S = s
T := t
A = 1
B = 2
undefine A
override undefine B
all:
	@echo "[$(intcmp 1,2)] [$(intcmp 2,2)] [$(intcmp 3,2,lt)] [$(intcmp 1,2,lt,eq,gt)] [$(intcmp 2,2,lt,eq)] [$(intcmp 3,2,lt,eq)] [$(intcmp -5,-05,a,b,c)]"
	@echo "[$(let a b,1 2 3,<$(a)|$(b)>)] [$(let a b c,1,<$(a)|$(b)|$(c)>)]"
	@echo "[$(flavor S)] [$(flavor T)] [$(flavor NOPE)] [$(flavor @)] [$(flavor MAKE)]"
	@echo "[$(notdir $(abspath a/./b/../c))] [$(abspath //x/ /)] [$(words $(realpath . nope out.txt sub/..)) $(notdir $(realpath out.txt))]"
	@echo "[$(subst $(newline), ,$(R))] [$(A)] [$(B)] [$(origin A)] [$(flavor B)]"
define newline


endef
