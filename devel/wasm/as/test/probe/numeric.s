# Constants at their edges, in every base and spelling, and numeric
# instructions of each group vcc uses.
	.section	.text.consts,"",@
	.globl	consts
	.type	consts,@function
consts:
	.functype	consts () -> ()
	i32.const	0
	drop
	i32.const	-1
	drop
	i32.const	63
	drop
	i32.const	64
	drop
	i32.const	-64
	drop
	i32.const	-65
	drop
	i32.const	2147483647
	drop
	i32.const	-2147483648
	drop
	i32.const	4294967295
	drop
	i32.const	0x7fffffff
	drop
	i32.const	017
	drop
	i32.const	0b101
	drop
	i64.const	0
	drop
	i64.const	9223372036854775807
	drop
	i64.const	-9223372036854775808
	drop
	i64.const	18446744073709551615
	drop
	i64.const	0xffffffffffffffff
	drop
	i64.const	-1
	drop
	f32.const	0x1.8p+1
	drop
	f32.const	-0x0p+0
	drop
	f32.const	0x1.fffffep+127
	drop
	f32.const	0x1p-149
	drop
	f32.const	0.5
	drop
	f32.const	1e10
	drop
	f32.const	infinity
	drop
	f32.const	-infinity
	drop
	f32.const	nan
	drop
	f32.const	-nan
	drop
	f32.const	NaN
	drop
	f32.const	-0x1.8p-1
	drop
	f64.const	0x1.fffffffffffffp+1023
	drop
	f64.const	0x0.0000000000001p-1022
	drop
	f64.const	-0x1.8p-1
	drop
	f64.const	0.25
	drop
	f64.const	1.5e3
	drop
	f64.const	-infinity
	drop
	f64.const	nan
	drop
	f64.const	-nan
	drop
	f64.const	0x0p+0
	drop
	end_function

	.section	.text.i32ops,"",@
	.globl	i32ops
	.type	i32ops,@function
i32ops:
	.functype	i32ops (i32, i32) -> ()
	local.get	0
	local.get	1
	i32.add
	drop
	local.get	0
	local.get	1
	i32.sub
	drop
	local.get	0
	local.get	1
	i32.mul
	drop
	local.get	0
	local.get	1
	i32.div_s
	drop
	local.get	0
	local.get	1
	i32.div_u
	drop
	local.get	0
	local.get	1
	i32.rem_s
	drop
	local.get	0
	local.get	1
	i32.rem_u
	drop
	local.get	0
	local.get	1
	i32.and
	drop
	local.get	0
	local.get	1
	i32.or
	drop
	local.get	0
	local.get	1
	i32.xor
	drop
	local.get	0
	local.get	1
	i32.shl
	drop
	local.get	0
	local.get	1
	i32.shr_s
	drop
	local.get	0
	local.get	1
	i32.shr_u
	drop
	local.get	0
	local.get	1
	i32.eq
	drop
	local.get	0
	local.get	1
	i32.ne
	drop
	local.get	0
	local.get	1
	i32.lt_s
	drop
	local.get	0
	local.get	1
	i32.lt_u
	drop
	local.get	0
	local.get	1
	i32.gt_s
	drop
	local.get	0
	local.get	1
	i32.gt_u
	drop
	local.get	0
	local.get	1
	i32.le_s
	drop
	local.get	0
	local.get	1
	i32.le_u
	drop
	local.get	0
	local.get	1
	i32.ge_s
	drop
	local.get	0
	local.get	1
	i32.ge_u
	drop
	local.get	0
	i32.eqz
	drop
	local.get	0
	i32.extend8_s
	drop
	local.get	0
	i32.extend16_s
	drop
	local.get	0
	i64.extend_i32_s
	drop
	local.get	0
	i64.extend_i32_u
	drop
	local.get	0
	f32.convert_i32_s
	drop
	local.get	0
	f32.convert_i32_u
	drop
	local.get	0
	f64.convert_i32_s
	drop
	local.get	0
	f64.convert_i32_u
	drop
	local.get	0
	f32.reinterpret_i32
	drop
	end_function

	.section	.text.i64ops,"",@
	.globl	i64ops
	.type	i64ops,@function
i64ops:
	.functype	i64ops (i64, i64) -> ()
	local.get	0
	local.get	1
	i64.add
	drop
	local.get	0
	local.get	1
	i64.sub
	drop
	local.get	0
	local.get	1
	i64.mul
	drop
	local.get	0
	local.get	1
	i64.div_s
	drop
	local.get	0
	local.get	1
	i64.div_u
	drop
	local.get	0
	local.get	1
	i64.rem_s
	drop
	local.get	0
	local.get	1
	i64.rem_u
	drop
	local.get	0
	local.get	1
	i64.and
	drop
	local.get	0
	local.get	1
	i64.or
	drop
	local.get	0
	local.get	1
	i64.xor
	drop
	local.get	0
	local.get	1
	i64.shl
	drop
	local.get	0
	local.get	1
	i64.shr_s
	drop
	local.get	0
	local.get	1
	i64.shr_u
	drop
	local.get	0
	local.get	1
	i64.eq
	drop
	local.get	0
	local.get	1
	i64.ne
	drop
	local.get	0
	local.get	1
	i64.lt_s
	drop
	local.get	0
	local.get	1
	i64.lt_u
	drop
	local.get	0
	local.get	1
	i64.gt_s
	drop
	local.get	0
	local.get	1
	i64.gt_u
	drop
	local.get	0
	local.get	1
	i64.le_s
	drop
	local.get	0
	local.get	1
	i64.le_u
	drop
	local.get	0
	local.get	1
	i64.ge_s
	drop
	local.get	0
	local.get	1
	i64.ge_u
	drop
	local.get	0
	i64.eqz
	drop
	local.get	0
	i64.extend8_s
	drop
	local.get	0
	i64.extend16_s
	drop
	local.get	0
	i64.extend32_s
	drop
	local.get	0
	i32.wrap_i64
	drop
	local.get	0
	f32.convert_i64_s
	drop
	local.get	0
	f32.convert_i64_u
	drop
	local.get	0
	f64.convert_i64_s
	drop
	local.get	0
	f64.convert_i64_u
	drop
	local.get	0
	f64.reinterpret_i64
	drop
	end_function

	.section	.text.f32ops,"",@
	.globl	f32ops
	.type	f32ops,@function
f32ops:
	.functype	f32ops (f32, f32) -> ()
	local.get	0
	local.get	1
	f32.add
	drop
	local.get	0
	local.get	1
	f32.sub
	drop
	local.get	0
	local.get	1
	f32.mul
	drop
	local.get	0
	local.get	1
	f32.div
	drop
	local.get	0
	local.get	1
	f32.eq
	drop
	local.get	0
	local.get	1
	f32.ne
	drop
	local.get	0
	local.get	1
	f32.lt
	drop
	local.get	0
	local.get	1
	f32.gt
	drop
	local.get	0
	local.get	1
	f32.le
	drop
	local.get	0
	local.get	1
	f32.ge
	drop
	local.get	0
	f32.neg
	drop
	local.get	0
	f32.sqrt
	drop
	local.get	0
	i32.trunc_sat_f32_s
	drop
	local.get	0
	i32.trunc_sat_f32_u
	drop
	local.get	0
	i64.trunc_sat_f32_s
	drop
	local.get	0
	i64.trunc_sat_f32_u
	drop
	local.get	0
	f64.promote_f32
	drop
	local.get	0
	i32.reinterpret_f32
	drop
	end_function

	.section	.text.f64ops,"",@
	.globl	f64ops
	.type	f64ops,@function
f64ops:
	.functype	f64ops (f64, f64) -> ()
	local.get	0
	local.get	1
	f64.add
	drop
	local.get	0
	local.get	1
	f64.sub
	drop
	local.get	0
	local.get	1
	f64.mul
	drop
	local.get	0
	local.get	1
	f64.div
	drop
	local.get	0
	local.get	1
	f64.eq
	drop
	local.get	0
	local.get	1
	f64.ne
	drop
	local.get	0
	local.get	1
	f64.lt
	drop
	local.get	0
	local.get	1
	f64.gt
	drop
	local.get	0
	local.get	1
	f64.le
	drop
	local.get	0
	local.get	1
	f64.ge
	drop
	local.get	0
	f64.neg
	drop
	local.get	0
	f64.sqrt
	drop
	local.get	0
	i32.trunc_sat_f64_s
	drop
	local.get	0
	i32.trunc_sat_f64_u
	drop
	local.get	0
	i64.trunc_sat_f64_s
	drop
	local.get	0
	i64.trunc_sat_f64_u
	drop
	local.get	0
	f32.demote_f64
	drop
	local.get	0
	i64.reinterpret_f64
	drop
	end_function

	.section	.text.choose,"",@
	.globl	choose
	.type	choose,@function
choose:
	.functype	choose (i32, i32, i32) -> (i32)
	local.get	0
	local.get	1
	local.get	2
	select	i32
	end_function
