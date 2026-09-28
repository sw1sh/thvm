#!/usr/bin/env python3
# Neural-selection phase 0: build the unit-equality (UEQ) problem pool.
#
# Pool = every TPTP problem whose header SPC field ends in UEQ (CNF, all
# clauses unit equalities) MINUS the CASC-J13 UEQ names (excluded by
# NAME: J13 reused earlier CASC problems).  The pool is split into train
# (80%) and held-out (20%) by a stable hash of the problem name, fixed
# before any training: sha256(name) -> first 8 hex digits mod 100.
#
# Usage: np0_pool.py <TPTP_root> <j13_names.txt> <out_pool.tsv>
# Output columns: name, relpath, status, spc, split
import hashlib
import os
import re
import sys


def split_of(name):
    h = int(hashlib.sha256(name.encode()).hexdigest()[:8], 16)
    return "train" if h % 100 < 80 else "heldout"


def header_fields(path):
    spc = status = None
    with open(path, "r", errors="replace") as f:
        for i, line in enumerate(f):
            if i > 200 or not line.startswith("%"):
                if i > 5 and not line.startswith("%"):
                    break
                continue
            m = re.match(r"%\s*SPC\s*:\s*(\S+)", line)
            if m:
                spc = m.group(1)
            m = re.match(r"%\s*Status\s*:\s*(\S+)", line)
            if m and status is None:
                status = m.group(1)
            if spc and status:
                break
    return spc, status


def main():
    root, j13_path, out_path = sys.argv[1:4]
    j13 = {l.strip() for l in open(j13_path) if l.strip()}
    rows = []
    n_ueq = 0
    probs = os.path.join(root, "Problems")
    for dom in sorted(os.listdir(probs)):
        d = os.path.join(probs, dom)
        if not os.path.isdir(d):
            continue
        for fn in sorted(os.listdir(d)):
            if not fn.endswith(".p"):
                continue
            path = os.path.join(d, fn)
            spc, status = header_fields(path)
            if spc is None or not spc.endswith("UEQ"):
                continue
            n_ueq += 1
            name = fn[:-2]
            if name in j13:
                continue
            rows.append((name, os.path.join("Problems", dom, fn),
                         status or "?", spc, split_of(name)))
    with open(out_path, "w") as f:
        for r in rows:
            f.write("\t".join(r) + "\n")
    n_tr = sum(1 for r in rows if r[4] == "train")
    print(f"ueq_total={n_ueq} j13_excluded={n_ueq - len(rows)} "
          f"pool={len(rows)} train={n_tr} heldout={len(rows) - n_tr}")


if __name__ == "__main__":
    main()
