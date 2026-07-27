#!/usr/bin/env python3
"""Score CheckProof on the open-world corpus (harvest.py + mutate.py).

  python3 score.py CORPUSDIR CHECKPROOF_BIN [--heldout-only] [--wall N]

Same scoring semantics as the ProoVer competition:
  bad-as-bad +2, good-as-good +1, abstain 0, good-as-bad -1,
  bad-as-good -10.

The corpus is split 70/30 by a stable hash of the file name; the 30%
held-out slice is NEVER to be used while fixing the checker.  Reports
both confusion matrices and per-bug-class outcomes, names every false
accept, and exits nonzero when the held-out slice has any false accept
(the hard requirement) .
"""
import collections
import os
import subprocess
import sys

def fnv(s):
    h = 1469598103934665603
    for c in s.encode():
        h = ((h ^ c) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return h

def heldout(fn):
    return fnv("split:" + fn) % 10 < 3

def verdict_of(binpath, path, wall):
    env = dict(os.environ)
    env.setdefault("THVM_PROOFCHECK_WALL", str(wall))
    env.setdefault("THVM_ATP_RSS_ABORT_MB", "5300")
    try:
        r = subprocess.run([binpath, "--szs", path], capture_output=True,
                           text=True, timeout=wall + 20, env=env)
    except subprocess.TimeoutExpired:
        return "Timeout"
    for ln in r.stdout.splitlines():
        if "SZS status" in ln:
            return ln.split("status", 1)[1].split("for")[0].strip()
    return "?"

POINTS = {
    ("buggy", "VerifiedBad"): 2, ("valid", "VerifiedGood"): 1,
    ("valid", "VerifiedBad"): -1, ("buggy", "VerifiedGood"): -10,
}

def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    corpus, binpath = sys.argv[1], os.path.abspath(sys.argv[2])
    only_held = "--heldout-only" in sys.argv
    wall = 30
    if "--wall" in sys.argv:
        wall = int(sys.argv[sys.argv.index("--wall") + 1])
    rows = [ln.split("\t") for ln in
            open(os.path.join(corpus, "manifest.tsv")).read().splitlines()[1:]]
    conf = {"dev": collections.Counter(), "held": collections.Counter()}
    bycls = collections.Counter()
    score = {"dev": 0, "held": 0}
    false_accepts, false_rejects = [], []
    for fn, gold, cls, prover in rows:
        split = "held" if heldout(fn) else "dev"
        if only_held and split != "held":
            continue
        sub = "valid" if gold == "valid" else "buggy"
        v = verdict_of(binpath, os.path.join(corpus, sub, fn), wall)
        conf[split][(gold, v)] += 1
        bycls[(cls, v)] += 1
        score[split] += POINTS.get((gold, v), 0)
        if gold == "buggy" and v == "VerifiedGood":
            false_accepts.append((split, fn))
        if gold == "valid" and v == "VerifiedBad":
            false_rejects.append((split, fn))
        print("  %-4s %-42s %-6s %s" % (split, fn, gold, v), flush=True)
    for split in ("dev", "held"):
        if only_held and split == "dev":
            continue
        n = sum(conf[split].values())
        print("\n== %s confusion (%d proofs, score %+d) ==" %
              (split, n, score[split]))
        for (g, v), c in sorted(conf[split].items()):
            print("  %-6s -> %-14s %d" % (g, v, c))
    print("\n== by class -> verdict ==")
    for (c, v), n in sorted(bycls.items()):
        print("  %-14s -> %-14s %d" % (c, v, n))
    print("\nfalse accepts (bad->Good, -10 each): %d" % len(false_accepts))
    for s, f in false_accepts:
        print("  [%s] %s" % (s, f))
    print("false rejects (valid->Bad, -1 each): %d" % len(false_rejects))
    for s, f in false_rejects:
        print("  [%s] %s" % (s, f))
    held_fa = [f for s, f in false_accepts if s == "held"]
    return 1 if held_fa else 0

if __name__ == "__main__":
    sys.exit(main())
