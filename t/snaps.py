#!/usr/bin/env python3

from functools import singledispatch
import sys
import argparse
import os
import shutil
import unittest

from pathlib import Path

# Snapshot testing program

sys.path.append("../ffi")

from iotalib import IotaLib, Node

iota = IotaLib("../ffi/libiota.so")

def valid_iota_file(path: str) -> str:
    path_obj = Path(path)

    if not path_obj.is_file():
        raise argparse.ArgumentTypeError(f"{path} is not a valid file")

    if path_obj.suffix != ".ta":
        raise argparse.ArgumentTypeError(
            f"{path} must be an iota file with .ta extension")

    return path

def extt(t):
    if t == "ast":
        return ".ast"
    else:
        return ".tbl"

def add_cmd(args):
    os.makedirs("snapshots/in", exist_ok=True)
    os.makedirs("snapshots/out", exist_ok=True)

    inp = Path("snapshots/in")
    assert inp.is_dir()

    outp = Path("snapshots/out")
    assert outp.is_dir()

    t = args.type
    for ss in args.snapshots:
        sp = Path(ss);
        assert sp.is_file()

        inpf = inp / sp.name
        if inpf.exists:
            # Does the snapshot associated with the type already exist?
            if extt(t) in [se.suffix for se in outp.iterdir() if se.stem == sp.stem]:
                print(f"{t} snapshot for file {ss} already exists", file=sys.stderr)
                sys.exit(1)

        # Copy the file to input path
        shutil.copy(sp, inpf)

        outf = outp / f"{sp.stem}{extt(t)}"
        assert t == "ast", "TODO symbol table dump"

        try:
            out = iota.parse_as(Node.SOURCE_FILE, inpf.read_text())
        except:
            print(f"{t} snapshot for file {ss} not created: parsing failed.", file=sys.stderr)
            continue

        outf.write_text(out)


def update_cmd(args):
    os.makedirs("snapshots/in", exist_ok=True)
    os.makedirs("snapshots/out", exist_ok=True)

    inp = Path("snapshots/in")
    assert inp.is_dir()

    outp = Path("snapshots/out")
    assert outp.is_dir()

    for ie in inp.iterdir():
        outputs = [oe for oe in outp.iterdir() if oe.stem == ie.stem]
        for output in outputs:
            if output.suffix == ".ast":
                try:
                    out = iota.parse_as(Node.SOURCE_FILE, ie.read_text())
                except:
                    print(f"ast snapshot for file {ie} not updated: parsing failed.", file=sys.stderr)
                    continue
                output.write_text(out)
            else:
                assert false, "TODO"

class SnapshotTests(unittest.TestCase):
    pass

def snapshot_test(name, inp, get_outp, expected) -> None:
    # Remove first prefix newline if it exists.
    def test(self):
        out = get_outp(inp)
        self.maxDiff = None
        self.assertEqual(expected, out)
    setattr(SnapshotTests, name, test)

def test_cmd(args):
    os.makedirs("snapshots/in", exist_ok=True)
    os.makedirs("snapshots/out", exist_ok=True)

    inp = Path("snapshots/in")
    assert inp.is_dir()

    outp = Path("snapshots/out")
    assert outp.is_dir()

    for ie in inp.iterdir():
        outputs = [oe for oe in outp.iterdir() if oe.stem == ie.stem]
        for output in outputs:
            if output.suffix == ".ast":
                def dump_ast(src: str) -> str:
                    return iota.parse_as(Node.SOURCE_FILE, src)
                name = f"test_{ie.stem}"
                snapshot_test(name, ie.read_text(), dump_ast, output.read_text())
            else:
                assert false, "TODO"

    unittest.main(argv=[sys.argv[0]])

def main():
    par = argparse.ArgumentParser(
        prog="snaps",
        description="Program to run Python based Iota tests")

    subpar = par.add_subparsers(required=True)

    par_add = subpar.add_parser("add", help="add new snapshots")
    par_add.set_defaults(handle=add_cmd)
    par_add.add_argument("-t", "--type",
        choices=("ast", "symbol_table"),
        default="ast",
        help="the data to extract and snapshot (default: ast)")
    par_add.add_argument("snapshots", nargs="+", type=valid_iota_file, help=".ta files")

    par_update = subpar.add_parser("update", help="update all snapshots")
    par_update.set_defaults(handle=update_cmd)

    par_test = subpar.add_parser("test", help="test all snapshots")
    par_test.set_defaults(handle=test_cmd)

    args = par.parse_args()
    args.handle(args)

if __name__ == "__main__":
    main()
