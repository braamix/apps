#!/bin/sh
# Regenerates the golden frames frames.mjs asserts, using upstream's own binary.
# Run it when the SDK's libzstd moves, and read the diff: a frame that changes
# is either upstream changing or this port drifting, and only the first is fine.
#
# Upstream zstd 1.6.0, built single-threaded with zlib and liblzma and without
# liblz4, which is the feature set of the Braam build:
#
#   cd <zstd-1.6.0>/programs
#   make HAVE_ZLIB=1 HAVE_LZMA=1 HAVE_LZ4=0 \
#        MOREFLAGS="-I$(brew --prefix xz)/include" \
#        LDFLAGS="-L$(brew --prefix xz)/lib" zstd
#
# --single-thread because libzstd under Braam has one worker and a multi-threaded
# encode does not produce the same frame.
set -e

ZSTD=${ZSTD:-/tmp/upzstd}
HERE=$(dirname "$0")

"$ZSTD" --version

node -e '
let s = "";
for (let i = 0; i < 500; i++)
    s += `line ${i}: the quick brown fox jumps over ${i % 7} lazy dogs\n`;
process.stdout.write(s);
' > "$HERE/frames.txt"

set -- "-1:1.zst" "-3:3.zst" "-19:19.zst" "--fast=3:fast3.zst" \
       "--long=20 -6:long20.zst" "-6 --no-check:nocheck.zst" \
       "-5 --format=xz:5.xz" "-5 --format=lzma:5.lzma"
for pair; do
    args=${pair%:*}
    out=${pair#*:}
    # shellcheck disable=SC2086
    "$ZSTD" -q --single-thread $args -c "$HERE/frames.txt" > "$HERE/frames.$out"
    echo "frames.$out  $args"
done
