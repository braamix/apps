#!/bin/sh
#
# Rebuild the host compiler in build/host-cc and hold it to the reference
# reference.sh made: the corpus byte for byte, and the driver's transcript,
# whose differences are printed for a reader to judge.
#
set -e
here=$(cd "$(dirname "$0")" && pwd)
top=$(cd "$here/../../.." && pwd)
dir=$top/build/cc-corpus
cmake -S "$here/.." -B "$top/build/host-cc" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$top/build/host-cc" -j8 2>&1 | grep -E 'error|warning' && exit 1
sh "$here/corpus.sh" "$top/build/host-cc" "$dir/tree" "$dir/now"
diff -r "$dir/ref" "$dir/now"
echo "corpus unchanged"
sh "$here/driver.sh" "$top/build/host-cc" "$dir/tree" "$dir/driver.now"
diff "$dir/driver.ref" "$dir/driver.now" || true
