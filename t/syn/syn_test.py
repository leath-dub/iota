#!/usr/bin/env python3

import os
import re


cwd = os.getcwd()

header = r"--- (.*)"


def run_tests(fp: str) -> None:
    with open(fp) as f:
        lines = f.readlines()

        m = re.search(header, lines[0])
        assert m
        name = m.group(1)

        lines = lines[1:]

        i = 0

        while i < len(lines):
            input = ""
            while i < len(lines):
                line = lines[i]
                i += 1
                if line == "===\n":
                    break
                input += line;

            expected = ""
            while i < len(lines):
                line = lines[i]
                if line.startswith("---"):
                    break
                expected += line
                i += 1

            run_test(input, expected)

            i += 1

for entry in os.listdir(cwd):
    fp = os.path.join(cwd, entry)
    if os.path.isfile(fp):
        _, ext = os.path.splitext(entry)
        if ext == ".t":
            run_tests(fp)
