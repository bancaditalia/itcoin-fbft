#!/usr/bin/env python3

"""
This script checks consistency of some third-party source code with the original copies.

Usage:

    python3 scripts/check-code-consistency.py

"""
import inspect
import urllib.request

from pathlib import Path

CUR_PATH = Path(inspect.getfile(inspect.currentframe())).parent  # type: ignore
ROOT_DIR = Path(CUR_PATH, "..").resolve().absolute()

THIRD_PARTY_DIR = ROOT_DIR / "src" / "thirdparty"

BITCOIN_CORE_BITCOIN_UTIL_CPP_URL = "https://raw.githubusercontent.com/bitcoin/bitcoin/master/src/bitcoin-util.cpp"
THIRD_PARTY_GRIND_CPP = THIRD_PARTY_DIR / "grind.cpp"


def _http_get(url) -> str:
    contents = urllib.request.urlopen(url).read().decode("utf-8")
    return contents


def check_grind() -> None:
    """Check that our grinding functions are the same of the ones in the main Bitcoin core repository (master branch)."""
    print("Let's compare grind_task() and verify that it is the same as the current one in bitcoin-core.")
    print(f"    Ours:   {THIRD_PARTY_GRIND_CPP}")
    print(f"    Theirs: {BITCOIN_CORE_BITCOIN_UTIL_CPP_URL}")
    print(f"Checking... ", end="")
    their_file = _http_get(BITCOIN_CORE_BITCOIN_UTIL_CPP_URL)
    our_file = THIRD_PARTY_GRIND_CPP.read_text()

    # extract our grind_task function
    our_grind_task_offset = 12
    our_grind_task_function = "\n".join(our_file.splitlines()[our_grind_task_offset: our_grind_task_offset + 25])

    # extract their grind_task function
    their_grind_task_offset = 85
    their_grind_task_function = "\n".join(their_file.splitlines()[their_grind_task_offset: their_grind_task_offset + 25])
    their_grind_task_function = their_grind_task_function.replace("static ", "")

    # compare the grind_task function
    assert our_grind_task_function == their_grind_task_function, \
        f"grind_task functions do not match.\nOurs: {our_grind_task_function}\nTheirs:\n{their_grind_task_function}"
    print("OK.")


def main() -> None:
    """Run the main function."""
    check_grind()
    print("Done.")


if __name__ == '__main__':
    main()
