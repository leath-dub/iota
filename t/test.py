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
        self.maxDiff = None
        self.assertEqual(ast, expected)
    setattr(SyntaxTests, f"test_{name}", test)

# Function to test if given input sources share the same AST
def syntax_test_equal(name: str = "unamed_test", lhs: str = "", rhs: str = "") -> None:
    lhs_ast = dump_ast(lhs)
    syntax_test(name, rhs, lhs_ast)

def load_test(path):
    path = pathlib.Path(path)
    with path.open() as f:
        code = f.read()
    exec(compile(code, str(path), 'exec'), globals())

def load_testdir(path):
    testdir = pathlib.Path("syn")
    for entr in testdir.iterdir():
        if entr.is_file() and entr.name.startswith("test_") and entr.suffix == ".py":
            load_test(str(entr))

load_testdir("syn")

if __name__ == "__main__":
    unittest.main()
