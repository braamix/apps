#!/bin/sh
#
# The driver's transcript: `cc -v` over a set of invocations, with as and ld
# stubbed, run from a fake installation of the passes in <build>, with the
# headers of <tree>. Two builds are compared with `diff`.
#
#   driver.sh <build> <tree> <out>
#
set -e
build=$(cd "$1" && pwd)
tree=$(cd "$2" && pwd)
out=$(cd "$(dirname "$3")" && pwd)/$(basename "$3")
inst=$(mktemp -d)
mkdir -p "$inst/bin" "$inst/share/vcc/wasm32-braam/include/sys" "$inst/share/vcc/wasm32-braam/lib"
cp "$build/cc" "$inst/bin/vcc"
for p in cpp parse lower; do cp "$build/$p" "$inst/bin/v$p"; done
cp "$build/genwasm" "$inst/bin/vgenwasm"
inc=$inst/share/vcc/wasm32-braam/include
cp "$tree"/libc/common/include/*.h "$tree"/libc/ilp32/include/*.h "$inc"
cp "$tree"/libc/wasm32/include/*.h "$inc"
cp "$tree"/libc/wasm32/braam/include/*.h "$inc"
cp "$tree"/libc/wasm32/braam/include/sys/*.h "$inc/sys"
printf 'x' > "$inst/share/vcc/wasm32-braam/lib/crt0.o"
printf 'x' > "$inst/share/vcc/wasm32-braam/lib/libc.a"
cat > "$inst/stub" <<'EOF'
#!/bin/sh
echo "stub: $(basename "$0") $*" | sed 's/vcc[A-Za-z0-9_]\{6\}/vccXXXXXX/g'
o=""; while [ $# -gt 0 ]; do [ "$1" = -o ] && o=$2; shift; done
[ -n "$o" ] && printf '\0asm\1\0\0\0' > "$o"
exit 0
EOF
chmod +x "$inst/stub"
mkdir "$inst/tools"; cp "$inst/stub" "$inst/tools/clang"; cp "$inst/stub" "$inst/tools/wasm-ld"
work=$(mktemp -d); cd "$work"
cat > hello.c <<'EOF'
#include <stdio.h>
coro(braam_call *) int main(int argc, char **argv) { await printf("hi %d\n", argc); return 0; }
EOF
cat > plain.c <<'EOF'
int main(void) { return 0; }
EOF
cat > bad.c <<'EOF'
int f(void) { return x; }
EOF
printf '\t.text\n' > a.s
printf '#define X 1\n\t.text\n' > b.S
run() {
    echo "=== $*"
    st=0
    VCC_AS="$inst/tools/clang" VCC_LD="$inst/tools/wasm-ld" "$inst/bin/vcc" -v "$@" >"$work/.log" 2>&1 || st=$?
    cat "$work/.log" |
        sed -e "s|/private$inst|INST|g" -e "s|$inst|INST|g" -e "s|$work|WORK|g" -e 's/vcc[A-Za-z0-9_]\{6\}/vccXXXXXX/g' -e "s|/private/var|/var|g" -e "s|$TMPDIR|TMP/|g"
    echo "status $st"; rm -f "$work/.log"
    ls
}
{
run -t wasm32-braam -E hello.c
run -t wasm32-braam -S hello.c
run -t wasm32-braam -c hello.c
run -t wasm32-braam hello.c -o hello
run -t wasm32-braam --initial-pages=16 hello.c -lm -L/x
run -t wasm32-braam -nostdlib hello.c
run -t wasm32-braam -c a.s b.S
run -t wasm32-braam plain.c
run -t wasm32-braam bad.c
run -t wasm32-braam -fmax-errors=1 -fno-diagnostics-show-caret bad.c
run -t wasm32-braam -DX=1 -UY -I/inc -O2 -g -Wall -std=c11 -S hello.c -o h.s
run -t wasm32 -S hello.c
run -t nosuch hello.c
run -S hello.c
run --help
} > "$out" 2>&1
rm -rf "$inst" "$work"
