#!/bin/bash
# Regenerate the ported .cpp files from tmp/ex/.
#
# This ran once, when the port was made; the .cpp files are the source from
# here on and are edited by hand. It is kept because it says exactly what the
# mechanical half of the port was: knr.py does the K&R conversion, the ifdef
# resolution and the coroutine pass, and one patch script per file does the
# edits that needed a decision.
set -e
cd "$(dirname "$0")/.."
PATCH=${PATCH:-tools/patch}

for f in ex ex_addr ex_cmds ex_cmds2 ex_cmdsub ex_get ex_io ex_re ex_set \
         ex_subr ex_unix ex_v ex_vadj ex_vget ex_vmain ex_voper ex_vops \
         ex_vops2 ex_vops3 ex_vput ex_vwind; do
    python3 tools/knr.py "tmp/ex/$f.c" > "$f.cpp" 2>/dev/null
    [ -f "$PATCH/$f.py" ] && (cd . && python3 "$PATCH/$f.py" >/dev/null)
done

# Ported code is clang-formatted like everything else here: upstream's
# identifiers, statements, comments and output are what a port keeps, and its
# whitespace is not.
command -v clang-format >/dev/null && clang-format -i *.cpp *.h
echo "regenerated"
