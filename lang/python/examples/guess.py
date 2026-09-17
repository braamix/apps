#!/pkg/bin/python
"""Guess the number, the way lang/mbasic/examples/guess.bas plays it.

GUESS_SEED in the environment pins the number, so a run can be repeated.
"""

import os
import random
import sys

LOW, HIGH = 1, 100


def ask(prompt):
    """A line, or None at the end of input."""
    try:
        return input(prompt)
    except EOFError:
        print()
        return None


def play(secret):
    """Tries taken, or None if the player left."""
    for tries in range(1, HIGH):
        line = ask("Your guess? ")
        if line is None:
            return None
        try:
            guess = int(line)
        except ValueError:
            print(f"{line!r} is not a number.")
            continue
        if guess < secret:
            print("Too low!")
        elif guess > secret:
            print("Too high!")
        else:
            return tries
    return None


def main():
    seed = os.environ.get("GUESS_SEED")
    random.seed(int(seed) if seed else None)

    print("Number guessing game")
    print("====================")
    while True:
        print()
        print(f"I'm thinking of a number between {LOW} and {HIGH}.")
        tries = play(random.randint(LOW, HIGH))
        if tries is None:
            break
        print()
        print(f"Correct! You got it in {tries} {'try' if tries == 1 else 'tries'}.")
        print()
        again = ask("Play again (y/n)? ")
        if again is None or not again.lower().startswith("y"):
            break
    print("Thanks for playing!")
    return 0


if __name__ == "__main__":
    sys.exit(main())
