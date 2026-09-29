;; Every kind of token, and the white space between them. The golden
;; file beside this one is what `as --tokens` makes of it.
(module $m
  (func $f (param $x i32) (result i32)
    local.get $x i32.const 1 i32.add)
  (memory 1) (i32.load8_u offset=0x10 align=1) (x)(y)()
)
;; Numbers: nat, int, float.
0 7 1_000 0x0 0xff_FF 0xDEAD_beef
+0 -7 +1_0 -0x1 +0xa_b
1. 1.5 1.5e10 1e-3 1E+3 1.e3 1_0.2_5e1_0 +1.5 -0.
0x1. 0x1.8 0x1p-3 0x1.8P+1 0xA.bp4 -0x1_0.0_1p1_0
inf +inf -inf nan -nan nan:0x1 +nan:0x7f_ffff
;; Words that are keywords, whatever the grammar makes of them.
a nan:canonical i32.const0 inf_ nan:0x offset=1_0 z!#%&'*+-./:<=>?@\^_`|~
;; Strings and their escapes.
"" "abc" "\t\n\r\\\'\"" "\00\7f\80\ff\Ab"
"\u{0}\u{7f}\u{80}\u{7ff}\u{800}\u{d7ff}\u{e000}\u{ffff}\u{10000}\u{10ffff}\u{1_F6_00}"
"café 😀" "; ;; (; ;) (@x"
;; Ids, plain and quoted.
$x $"x" $0 $$ $x.y:z $!#%&'*+-./:<=>?@\^_`|~ $"a b" $"\u{e9}" $"café"
;; Comments, which nest, and annotations, which are one token each.
(; a (; b ;) c
   ;; not a line comment (; still nested ;) ;) after
(@a) (@name "n") (@"q u" x) (@x (y (z ")" (; ) ;) $a $"b")) ;; )
  ) (@x (@y z) (@"w") ( @ ) (@) @@ , ; [ ] { } "\u{1}" "é") (@b
	) end
;; Line ends: CR, CR LF, and none at the end.cr
crlf
	tab ;; last