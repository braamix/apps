#!/bin/sh
#
# The reference corpus (Plan.md, step 1): every C source of a tree compiled by
# the passes to assembly, so two builds of the compiler can be compared with
# `diff -r`.
#
#   corpus.sh <passes> <tree> <out>
#
# <passes> holds cpp, parse, lower and genwasm; <tree> is a copy of devel/cc,
# whose sources and headers are the inputs. Each source a/b.c gives
# <out>/a_b.c.s, or <out>/a_b.c.err with the diagnostics and the status of the
# pass that failed. Braam's programs and runtime are compiled for
# wasm32-braam, everything else for wasm32.
#
set -e

if [ "$1" = --one ]; then
    bin=$2 out=$3 src=$4
    name=$(echo "$src" | tr / _)
    work=$out/$name.tmp
    mkdir -p "$work"
    case $src in
    test/programs/braam_tests/* | libc/wasm32/braam/* | docs/examples/*)
        target=wasm32-braam
        inc="-Ilibc/wasm32/braam/include"
        ;;
    *)
        target=wasm32
        inc=""
        ;;
    esac
    inc="$inc -Ilibc/wasm32/include -Ilibc/ilp32/include -Ilibc/common/include"
    case $src in
    libc/*) ;;
    *) inc="$inc -Ilibutil -Iast -Iscanner -Iparser -Itac -Isemantic -Itranslator"
       inc="$inc -Ioptimize -Ibackend/common -Ibackend/wasm -Icpp" ;;
    esac
    status=0
    "$bin/cpp" -t $target -nostdinc $inc "$src" "$work/x.i" >"$work/log" 2>&1 &&
        "$bin/parse" "$work/x.i" "$work/x.ast" >>"$work/log" 2>&1 &&
        "$bin/lower" -t $target "$work/x.ast" "$work/x.tac" >>"$work/log" 2>&1 &&
        "$bin/genwasm" "$work/x.tac" "$work/x.s" >>"$work/log" 2>&1 || status=$?
    if [ $status = 0 ]; then
        mv "$work/x.s" "$out/$name.s"
    else
        echo "status $status" >>"$work/log"
        mv "$work/log" "$out/$name.err"
    fi
    rm -rf "$work"
    exit 0
fi

bin=$(cd "$1" && pwd)
tree=$(cd "$2" && pwd)
mkdir -p "$3"
out=$(cd "$3" && pwd)
self=$(cd "$(dirname "$0")" && pwd)/$(basename "$0")

rm -f "$out"/*.s "$out"/*.err
cd "$tree"
ls test/programs/*/*.c libc/*/*.c libc/wasm32/braam/*.c docs/examples/*.c \
    */*.c backend/*/*.c 2>/dev/null | sort -u |
    xargs -n 1 -P 8 sh "$self" --one "$bin" "$out"
echo "$(ls "$out" | grep -c '\.s$') compiled, $(ls "$out" | grep -c '\.err$') refused"
