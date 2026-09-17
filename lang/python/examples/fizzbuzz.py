#!/pkg/bin/python
"""Fizz buzz. The count is the first argument, and defaults to 20."""

import sys


def fizzbuzz(n):
    """"Fizz" for a multiple of 3, "Buzz" for one of 5, both for one of 15."""
    word = "Fizz" * (n % 3 == 0) + "Buzz" * (n % 5 == 0)
    return word or str(n)


def main(argv):
    if len(argv) > 1:
        try:
            count = int(argv[1])
        except ValueError:
            print(f"{argv[0]}: {argv[1]} is not a number", file=sys.stderr)
            return 1
    else:
        count = 20
    print(" ".join(fizzbuzz(n) for n in range(1, count + 1)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
