#!/pkg/bin/python
"""Hello, world -- and the two lines after it that make it Python."""

import sys


def main():
    print("Hello, world!")
    print(f"This is {sys.implementation.name} {sys.version.split()[0]}.")
    for i, word in enumerate("the quick brown fox".split(), start=1):
        print(f"{i}. {word} ({len(word)} letters)")


if __name__ == "__main__":
    main()
