#!/bin/sh
# Builds the examples in the current directory: hello, echo, cat,
# hello2, fib and primes. ld finds libw.a in the package's lib/.
#
# To build the library here instead, from proc.s, fmt.s and args.s,
# which ld then finds first:
#
#   as proc.s fmt.s args.s
#   ar rc libw.a proc.o fmt.o args.o
#
# and link with -L. before -lw.
set -e
for p in hello echo cat hello2 fib primes; do
    as $p.s
done
ld hello.o -o hello
for p in echo cat hello2 fib primes; do
    ld $p.o -lw -o $p
done
