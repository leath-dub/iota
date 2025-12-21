#!/usr/bin/env python3

import textwrap
import unittest
import pathlib
import subprocess
import argparse
import sys

sys.path.append("../ffi")

from iotalib import IotaLib, Node

iota = IotaLib("../ffi/libiota.so")

class SyntaxTests(unittest.TestCase):
    pass

def dump_ast(source: str, node: (str | None) = None) -> str:
    t = Node.SOURCE_FILE
    if node:
        t = getattr(Node, node)
    return iota.parse_as(t, source)

def syntax_test(name: str = "unamed_test", source: str = "", expected: str = "", node: (str | None) = None) -> None:
    inpf = pathlib.Path("snapshots/in") / f"{name}.ta"
    inpf.write_text(textwrap.dedent(source))

    outf = pathlib.Path("snapshots/out") / f"{name}.ast"
    outf.write_text(dump_ast(source, "SOURCE_FILE"))

    # source = textwrap.dedent(source)
    # expected = textwrap.dedent(expected)
    # # Remove first prefix newline if it exists.
    # if len(expected) != 0 and expected[0] == "\n":
    #     expected = expected[1:]
    # def test(self):
    #     ast = dump_ast(source, node)
    #     self.maxDiff = None
    #     self.assertEqual(ast, expected)
    # setattr(SyntaxTests, f"test_{name}", test)

# Function to test if given input sources share the same AST
def syntax_test_equal(name: str = "unamed_test", lhs: str = "", rhs: str = "", node: (str | None) = None) -> None:
    lhs_ast = dump_ast(lhs, node)
    syntax_test(name, rhs, lhs_ast, node)

def load_test(path):
    path = pathlib.Path(path)
    with path.open() as f:
        code = f.read()
    exec(compile(code, str(path), 'exec'), globals())

def main():
    parser = argparse.ArgumentParser(description="Program to run Python based Iota tests")
    parser.add_argument("test_file", help="The file to test.")
    args = parser.parse_args()
    load_test(args.test_file)
    # unittest.main(argv=[sys.argv[0]])

if __name__ == "__main__":
    main()
