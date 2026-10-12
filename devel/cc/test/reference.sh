#!/bin/sh
#
# The reference the trimmed compiler is held to: vcc as imported, built on
# the host, and what it makes of the corpus and of the driver's invocations.
#
#   reference.sh <vcc checkout> [<commit>]
#
# <commit> is the import, 0b37290 by default. Everything goes in
# build/cc-corpus: tree/ is devel/cc at the import with vcc's test snippets
# added to it, refbin/ its build, ref/ its corpus and driver.ref its
# transcript. check.sh compares the current tree with them.
#
set -e
vcc=$(cd "$1" && pwd)
commit=${2:-0b37290}
here=$(cd "$(dirname "$0")" && pwd)
top=$(cd "$here/../../.." && pwd)
dir=$top/build/cc-corpus
rm -rf "$dir/tree" "$dir/refbin" "$dir/ref"
mkdir -p "$dir/tree"
git -C "$top" archive "$commit" devel/cc | tar -x -C "$dir/tree" --strip-components=2
python3 -I "$here/snippets.py" "$vcc" "$dir/tree/test/snippets"
cmake -S "$dir/tree" -B "$dir/refbin" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$dir/refbin" -j8 >/dev/null
sh "$here/corpus.sh" "$dir/refbin" "$dir/tree" "$dir/ref"
sh "$here/driver.sh" "$dir/refbin" "$dir/tree" "$dir/driver.ref"
