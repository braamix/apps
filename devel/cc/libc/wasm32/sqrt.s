# double sqrt(double), float sqrtf(float): the wasm instructions, correctly rounded as
# C requires.  The code generator emits f64.sqrt for a call of sqrt itself; these serve
# its address and code compiled with -fno-builtin.

	.section	.text.sqrt,"",@
	.globl	sqrt
	.type	sqrt,@function
sqrt:
	.functype	sqrt (f64) -> (f64)
	local.get	0
	f64.sqrt
	end_function

	.section	.text.sqrtf,"",@
	.globl	sqrtf
	.type	sqrtf,@function
sqrtf:
	.functype	sqrtf (f32) -> (f32)
	local.get	0
	f32.sqrt
	end_function
