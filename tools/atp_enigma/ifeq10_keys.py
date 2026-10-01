#!/usr/bin/env python3
"""Renaming-blind keys of CNF problem files, for the J13 duplicate check.

Usage: ifeq10_keys.py TPTP_ROOT LIST

LIST names one problem file per line.  For each, its include()s are
resolved (against the file's directory, then TPTP_ROOT) and one line is
printed: PATH, a tab and ifeq10_diff.problem_key of its cnf records, or
PATH, a tab, ERROR, a tab and the reason when the file cannot be read,
is not CNF, or holds a term nested too deep.  Two problems that are equal
modulo variable and symbol renaming, clause order and clause names get one
key (the converse need not hold).  make_ifeq10_corpus.sh keys the J13
problem files and every emitted encoding with it, and drops an encoding
whose key is a J13 key.  Text processing only.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import ifeq10_diff  # noqa: E402


def key_line(path, root):
    try:
        return "%s\t%s" % (path, ifeq10_diff.problem_key(ifeq10_diff.read_problem(path, root)))
    except (OSError, ifeq10_diff.ParseError) as e:
        return "%s\tERROR\t%s" % (path, str(e).replace("\t", " ").replace("\n", " "))
    except RecursionError:
        return "%s\tERROR\ta term is nested deeper than %d levels" % (path, ifeq10_diff.DEPTH_LIMIT)
    except MemoryError:
        return "%s\tERROR\tout of memory" % path


def main():
    if len(sys.argv) != 3:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    root = sys.argv[1]
    try:
        with open(sys.argv[2], encoding="utf-8") as fp:
            paths = [ln.rstrip("\n") for ln in fp if ln.strip()]
    except OSError as e:
        print("ifeq10_keys: %s" % e, file=sys.stderr)
        return 2
    for path in paths:
        print(key_line(path, root), flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(ifeq10_diff.run_deep(main))
