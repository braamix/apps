# Globals: imported of each type, mutable or not, under names of their own;
# and defined here.
	.globaltype	__stack_pointer, i32
	.globaltype	counter, i64, immutable
	.import_module	counter, host
	.import_name	counter, ticks
	.globaltype	gf, f32
	.globaltype	gd, f64
	.globaltype	mine, i32
	.globaltype	mine64, i64, immutable

	.section	.text.globals,"",@
	.globl	globals
	.type	globals,@function
globals:
	.functype	globals () -> (i64)
	global.get	__stack_pointer
	i32.const	16
	i32.sub
	global.set	__stack_pointer
	global.get	gf
	global.set	gf
	global.get	gd
	global.set	gd
	global.get	mine
	global.set	mine
	global.get	mine64
	drop
	global.get	counter
	end_function

	.section	.data.globals,"",@
	.globl	mine
mine:
mine64:
