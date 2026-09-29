#!/bin/sh
# Builds the examples in the current directory: hello, echo, cat, fib
# and primes.
set -e
for p in hello echo cat; do
    as $p.s
    ld $p.o -o $p
done
as crt.s fmt.s args.s
ar rc libw.a fmt.o args.o
for p in fib primes; do
    as $p.s
    ld crt.o $p.o -L. -lw -o $p
done
