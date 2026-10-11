#!/bin/sh
# Rebuilds corpus/: three sources as vcc's wasm32-braam toolchain has them,
# which test/objects.mjs holds as to llvm-mc on beside probe/. crt0.s is
# written by hand, doprnt.s is library code, and notes.s a whole program;
# the rest of vcc's runtime adds nothing that these and probe/ do not.
#
#   sh mkcorpus.sh <vcc checkout>
#
# The checkout must be built (make), and vcc installed, for vcc -S.
set -e
vcc=${1:?usage: mkcorpus.sh <vcc checkout>}
here=$(cd "$(dirname "$0")" && pwd)
out=$here/corpus
rm -rf "$out"
mkdir -p "$out"
cp "$vcc/libc/wasm32/braam/exports.s" "$out/crt0.s"
cp "$vcc/build/libc/wasm32/doprnt.s" "$out/doprnt.s"
vcc -t wasm32-braam -S -o "$out/notes.s" "$vcc/docs/examples/notes.c"
{
    echo "vcc $(git -C "$vcc" rev-parse --short HEAD)"
    echo "crt0.s: libc/wasm32/braam/exports.s"
    echo "doprnt.s: build/libc/wasm32/doprnt.s, vcc -t wasm32"
    echo "notes.s: docs/examples/notes.c, vcc -t wasm32-braam -S"
} > "$out/MANIFEST"
