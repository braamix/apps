# The math functions wasm answers with one instruction each, which the SDK's
# native.c has from clang's builtins, and i64.clz for musl's fma.c. A call of
# sqrt itself is f64.sqrt already; these serve every other call and an address.
# nearbyint is rint, there being no floating-point environment.

	.section	.text.sqrt,"",@
	.globl	sqrt
	.type	sqrt,@function
sqrt:
	.functype	sqrt (f64) -> (f64)
	local.get	0
	f64.sqrt
	end_function

	.section	.text.fabs,"",@
	.globl	fabs
	.type	fabs,@function
fabs:
	.functype	fabs (f64) -> (f64)
	local.get	0
	f64.abs
	end_function

	.section	.text.floor,"",@
	.globl	floor
	.type	floor,@function
floor:
	.functype	floor (f64) -> (f64)
	local.get	0
	f64.floor
	end_function

	.section	.text.ceil,"",@
	.globl	ceil
	.type	ceil,@function
ceil:
	.functype	ceil (f64) -> (f64)
	local.get	0
	f64.ceil
	end_function

	.section	.text.trunc,"",@
	.globl	trunc
	.type	trunc,@function
trunc:
	.functype	trunc (f64) -> (f64)
	local.get	0
	f64.trunc
	end_function

	.section	.text.rint,"",@
	.globl	rint
	.type	rint,@function
rint:
	.functype	rint (f64) -> (f64)
	local.get	0
	f64.nearest
	end_function

	.section	.text.nearbyint,"",@
	.globl	nearbyint
	.type	nearbyint,@function
nearbyint:
	.functype	nearbyint (f64) -> (f64)
	local.get	0
	f64.nearest
	end_function

	.section	.text.copysign,"",@
	.globl	copysign
	.type	copysign,@function
copysign:
	.functype	copysign (f64, f64) -> (f64)
	local.get	0
	local.get	1
	f64.copysign
	end_function

	.section	.text.sqrtf,"",@
	.globl	sqrtf
	.type	sqrtf,@function
sqrtf:
	.functype	sqrtf (f32) -> (f32)
	local.get	0
	f32.sqrt
	end_function

	.section	.text.fabsf,"",@
	.globl	fabsf
	.type	fabsf,@function
fabsf:
	.functype	fabsf (f32) -> (f32)
	local.get	0
	f32.abs
	end_function

	.section	.text.floorf,"",@
	.globl	floorf
	.type	floorf,@function
floorf:
	.functype	floorf (f32) -> (f32)
	local.get	0
	f32.floor
	end_function

	.section	.text.ceilf,"",@
	.globl	ceilf
	.type	ceilf,@function
ceilf:
	.functype	ceilf (f32) -> (f32)
	local.get	0
	f32.ceil
	end_function

	.section	.text.truncf,"",@
	.globl	truncf
	.type	truncf,@function
truncf:
	.functype	truncf (f32) -> (f32)
	local.get	0
	f32.trunc
	end_function

	.section	.text.rintf,"",@
	.globl	rintf
	.type	rintf,@function
rintf:
	.functype	rintf (f32) -> (f32)
	local.get	0
	f32.nearest
	end_function

	.section	.text.nearbyintf,"",@
	.globl	nearbyintf
	.type	nearbyintf,@function
nearbyintf:
	.functype	nearbyintf (f32) -> (f32)
	local.get	0
	f32.nearest
	end_function

	.section	.text.copysignf,"",@
	.globl	copysignf
	.type	copysignf,@function
copysignf:
	.functype	copysignf (f32, f32) -> (f32)
	local.get	0
	local.get	1
	f32.copysign
	end_function

	.section	.text.__clz64,"",@
	.globl	__clz64
	.type	__clz64,@function
__clz64:
	.functype	__clz64 (i64) -> (i32)
	local.get	0
	i64.clz
	i32.wrap_i64
	end_function
