# Functions: parameters, results, locals in runs of one type, and the
# instructions on locals.
	.section	.text.add,"",@
	.globl	add
	.type	add,@function
add:
	.functype	add (i32, i32) -> (i32)
	local.get	0
	local.get	1
	i32.add
	end_function

	.section	.text.locals,"",@
	.type	locals,@function
locals:
	.functype	locals (i64, f32) -> (f64)
	.local	i32, i32, i64, f32, f64, f64, i32, i32, i32
	local.get	0
	local.set	4
	local.get	1
	local.tee	5
	drop
	local.get	6
	return
	end_function

	.section	.text.nothing,"",@
	.globl	nothing
	.type	nothing,@function
nothing:
	.functype	nothing () -> ()
	nop
	end_function

	.functype	add2 (i32) -> (i32)
	.section	.text.recur,"",@
	.globl	recur
	.type	recur,@function
recur:
	.functype	recur (i32) -> (i32)
	local.get	0
	call	add2
	end_function

	.section	.text.add2,"",@
	.type	add2,@function
add2:
	.functype	add2 (i32) -> (i32)
	local.get	0
	local.get	0
	call	add
	end_function
