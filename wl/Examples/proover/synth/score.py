#!/usr/bin/env python3
"""Score FindProof --check against the synthesized gold labels.

  python3 score.py [corpus_dir] [findproof_binary]

corpus_dir defaults to this directory (run generate.py first); the binary
defaults to the repo's bin/FindProof.  Reports the confusion matrix, the
per-bug-class catch rate, the ProoVer competition score, and -- the number
that matters -- the count of catastrophic errors (a valid proof rejected as
VerifiedBad, or a buggy proof accepted as VerifiedGood).
"""
import os, sys, subprocess, collections

HERE = os.path.dirname(os.path.abspath(__file__))
CORPUS = sys.argv[1] if len(sys.argv) > 1 else HERE
FP = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, "../../../../bin/FindProof")
FP = os.path.abspath(FP)

def verdict_of(path):
    r = subprocess.run([FP, "--check", "--szs", path],
                       capture_output=True, text=True, timeout=60)
    for ln in r.stdout.splitlines():
        if "SZS status" in ln:
            return ln.split("status", 1)[1].split("for")[0].strip()
    return "?"

def main():
    man = os.path.join(CORPUS, "manifest.tsv")
    rows = [l.rstrip("\n").split("\t") for l in open(man)][1:]
    conf = collections.Counter()
    bug = collections.Counter()
    bad = []
    for fn, gold, cls in rows:
        path = os.path.join(CORPUS, "valid" if gold == "valid" else "buggy", fn)
        v = verdict_of(path)
        conf[(gold, v)] += 1
        if gold == "buggy":
            bug[(cls, v)] += 1
        if gold == "valid" and v == "VerifiedBad":
            bad.append((fn, "valid -> VerifiedBad (-10)"))
        if gold == "buggy" and v == "VerifiedGood":
            bad.append((fn, "buggy -> VerifiedGood (-10)"))

    print("== confusion (gold -> verdict) ==")
    for (g, v), n in sorted(conf.items()):
        print(f"  {g:6s} -> {v:16s} {n}")
    print("\n== buggy proofs by bug class -> verdict ==")
    for (c, v), n in sorted(bug.items()):
        print(f"  {c:10s} -> {v:16s} {n}")

    nv = sum(n for (g, _), n in conf.items() if g == "valid")
    nb = sum(n for (g, _), n in conf.items() if g == "buggy")
    score = 0
    for (g, v), n in conf.items():
        if g == "valid":
            score += n * (1 if v == "VerifiedGood" else -1 if v == "VerifiedBad" else 0)
        else:
            score += (2 * n if v == "VerifiedBad" else -10 * n if v == "VerifiedGood" else 0)
    print(f"\n== {nv} valid + {nb} buggy = {nv + nb} proofs ==")
    print(f"== ProoVer score: {score} (max {nv + 2 * nb}) ==")
    print(f"== catastrophic errors: {len(bad)} ==")
    for fn, why in bad:
        print("   !!", fn, why)
    return 1 if bad else 0

if __name__ == "__main__":
    sys.exit(main())
