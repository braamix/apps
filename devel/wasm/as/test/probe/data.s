# Data: sections of each kind and alignment, values of each width, symbols
# in data, strings with escapes, fill and alignment with a limit.
	.type	bytes,@object
	.section	.rodata.bytes,"",@
bytes:
	.int8	0
	.int8	-128
	.int8	255
	.int16	-32768
	.int16	65535
	.int32	-2147483648
	.int32	4294967295
	.int64	-9223372036854775808
	.int64	18446744073709551615
	.int32	0x7fc00000
	.int64	0x3ff0000000000000
	.int32	2+3-1
	.size	bytes, 47

	.type	text,@object
	.section	.rodata.text,"S",@
text:
	.ascii	"tab\there, quote\" backslash\\ nl\n"
	.ascii	"octal\101\0\377 hex\x41\x7f", "second"
	.asciz	"zero"
	.string	"also", "two"
	.ascii	""
	.size	text, 72

	.type	pointers,@object
	.section	.data.pointers,"",@
	.globl	pointers
	.p2align	3, 0x0
pointers:
	.int32	bytes
	.int32	text+5
	.int32	bytes-1
	.int32	0
	.int64	pointers+8
	.int32	.Lprivate
	.int32	elsewhere
	.int32	elsewhere+16
	.int32	weakdata
	.size	pointers, 40

	.section	.rodata..Lprivate,"",@
.Lprivate:
	.ascii	"private"
	.size	.Lprivate, 7

	.type	zeros,@object
	.section	.bss.zeros,"",@
	.globl	zeros
	.hidden	zeros
	.p2align	4
zeros:
	.skip	3
	.p2align	2, 0x55, 1
	.skip	3, 0xaa
	.p2align	3, 0x66, 2
	.skip	10
	.size	zeros, 17

	.type	weakdata,@object
	.section	.data.weakdata,"R",@
	.weak	weakdata
weakdata:
	.int32	7
	.size	weakdata, 4

	.section	.data.passive,"p",@
passive:
	.int8	1
	.int8	2
	.size	passive, 1

	.data
plain:
	.int16	513
	.size	plain, 2

	.section	.text.addr,"",@
	.globl	addr
	.type	addr,@function
addr:
	.functype	addr () -> (i32)
	i32.const	elsewhere+4
	i32.const	pointers
	i32.add
	i32.const	plain-2
	i32.add
	end_function
