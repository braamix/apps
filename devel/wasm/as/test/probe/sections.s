# The lexical forms and the sections a label makes: a function begun in
# .text gets a section of its own, a private label makes none, and
# statements may share a line.
	.file	"sections.c"
	.ident	"by hand"
	.text
	.globl	auto
	.type	auto,@function
auto:                                   // a C++ comment
	.functype	auto () -> (i32)
	/* a C comment */ i32.const 1 ; i32.const 2
	i32.add
.Linside:
	end_function

	.type	second,@function
second:	.functype	second () -> ()
	end_function

	.section	.text.third,"",@
	.type	third,@function
third:
	.functype	third () -> ()
	.size	third, 4
	end_function

	.data
	.type	label,@object
label:	.int32	label
	.size	label, 4
