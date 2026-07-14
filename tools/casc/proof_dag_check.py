#!/usr/bin/env python3
"""Closed-DAG check for SZS proof output (CASC-J13 solution compliance).

Reads a FindProof --proof output (or any TSTP derivation), collects every
fof(...)/cnf(...) record between the SZS output delimiters, and verifies:
  1. every parent cited by an inference(...) source exists as a record
     (the parent list is the LAST innermost [...] group of the record,
     which skips info-list annotations like new_symbols(skolem, [..]));
  2. records without an inference source are file(...)/introduced(...)
     leaves;
  3. the derivation ends at $false / a contradiction record.
Exit 0 = closed DAG; 1 = violations (each printed); 2 = no proof found.
"""
import re
import sys


def main(path):
    text = open(path).read()
    m = re.search(r"% SZS output start.*?\n(.*)% SZS output end", text, re.S)
    if m is None:
        print(f"{path}: no SZS output block")
        return 2
    body = m.group(1)
    recs = re.findall(r"^((?:fof|cnf)\((\w+),.*?\)\.)\s*$", body, re.S | re.M)
    names = {name for _, name in recs}
    bad = n_leaf = 0
    for full, name in recs:
        if "inference(" not in full:
            if "file(" not in full and "introduced(" not in full:
                print(f"{path}: {name}: no source (not a file/introduced leaf)")
                bad += 1
            else:
                n_leaf += 1
            continue
        groups = re.findall(r"\[([^\[\]]*)\]", full)
        if not groups:
            print(f"{path}: {name}: inference with no parent list")
            bad += 1
            continue
        for parent in re.findall(r"\b\w+\b", groups[-1]):
            if parent not in names:
                print(f"{path}: {name}: missing parent {parent}")
                bad += 1
    if "$false" not in body and "contradiction" not in body:
        print(f"{path}: derivation does not end at $false")
        bad += 1
    print(f"{path}: {len(names)} records, {n_leaf} leaves, "
          f"{'CLOSED' if bad == 0 else f'{bad} violation(s)'}")
    return 0 if bad == 0 else 1


if __name__ == "__main__":
    sys.exit(max(main(p) for p in sys.argv[1:]))
