#!/bin/sh
# Builds the examples in the current directory: hello, echo, cat,
# hello2, fib and primes. ld finds crt.o and libw.a in the package's
# lib/.
#
# To build the library here instead, from crt.s, proc.s, fmt.s and
# args.s, which ld then finds first:
#
#   as crt.s proc.s fmt.s args.s
#   ar rc libw.a proc.o fmt.o args.o
#
# and link with -L. before -lw.
set -e
for p in hello echo cat hello2 fib primes; do
    as $p.s
done
for p in hello echo cat; do
    ld $p.o -lw -o $p
done
for p in hello2 fib primes; do
    ld crt.o $p.o -lw -o $p
done
