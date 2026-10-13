FOO = b
BAR = $(FOO)x
export BAR
unexport QUX
Z = 1
export W = w
all:
	echo "[$$FOO] [$$BAR] [$$QUX] [$$Z] [$$CL] [$$D] [$$W]"
