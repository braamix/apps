#!/bin/sh
# The ERE engine against the host's, natively. No kernel and no SDK.
set -e
here=$(dirname "$0")
out=${TMPDIR:-/tmp}/eh-regex-test
obj=${TMPDIR:-/tmp}/eh-regex-oracle.o
san="-fsanitize=address,undefined"
cc -std=c11 -g $san -Wall -Wextra -c -o "$obj" "$here/regex_oracle.c"
c++ -std=c++20 -g $san -Wall -Wextra \
    -o "$out" "$here/regex_test.cpp" "$here/../regex.cpp" "$obj"
exec "$out"
