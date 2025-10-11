#!/usr/bin/env python3

import textwrap
import unittest
import pathlib
import subprocess

class SyntaxTests(unittest.TestCase):
    pass

def dump_ast(source: str) -> str:
    return subprocess.run(
        ["syn/dump_ast"],
        input=bytes(source, "utf-8"),
        capture_output=True
    ).stdout.decode()

def syntax_test(name: str = "unamed_test", source: str = "", expected: str = "") -> None:
    source = textwrap.dedent(source)
    expected = textwrap.dedent(expected)
    # Remove first prefix newline if it exists.
    if len(expected) != 0 and expected[0] == "\n":
        expected = expected[1:]
    def test(self):
        ast = dump_ast(source)
        self.assertEqual(ast, expected)
    setattr(SyntaxTests, f"test_{name}", test)

def load_test(path):
    path = pathlib.Path(path)
    with path.open() as f:
        code = f.read()
    exec(compile(code, str(path), 'exec'), globals())

load_test("syn/empty.py")
load_test("syn/function.py")

if __name__ == "__main__":
    unittest.main()
