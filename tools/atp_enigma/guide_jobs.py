#!/usr/bin/env python3
# The job list of a guided harvest (tools/atp_enigma/guide_harvest.sh): every
# TRAIN and TRAIN-X (problem, lane) pair of the given lanes whose 60 s lane
# matrix row shows the lane searched the problem (class proved, wall,
# gaveup-saturated, step-cap or memory-guard: not an exit-fast gate, a
# crash or a killed run), so failed runs come too (their hindsight and
# cross-lane labels).  TRAIN is the pool's train split less the J13
# stem-mates and the DEV-T half of the train tail (decisions_5_3.txt: 610
# problems); TRAIN-X is the synthetic -10 corpus's training split (A22).
# Text processing only.
#
# A job's dump samples about one pick in `every`, every = max(1,
# ceil(steps / PICKS)) with steps its matrix row's selections, so a run
# like its matrix run dumps about PICKS picks spread over the whole run.
# Pool paths are Problems/..., corpus paths TRAIN-X/Problems/...: the
# harvest's TPTP root holds the pool root's Problems/ and Axioms/ and the
# corpus's TRAIN-X/.  Jobs are dealt to SLOTS files, problems in (split,
# name) order round robin, every lane of a problem in one slot, ordered
# within a slot by sha256(name TAB lane).
#
# Usage: guide_jobs.py <matrix.tsv> <problems.tsv> <dev_t.tsv> <xmatrix.tsv>
#          <xproblems.tsv> <lanes> <out_dir> [picks=1000] [slots=2]
#   lanes: comma-separated lane names
# Writes <out_dir>/jobs.tsv (name, path, split, lane, every), slot_<k>.tsv,
# problems.txt (the paths the pod needs) and summary.txt.  Refuses a matrix
# with a j13 row and a job whose stem is a J13 stem.
import hashlib
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from matrix_tables import stem, x_stem  # noqa: E402

SEARCHED = ("proved", "wall", "gaveup-saturated", "step-cap", "memory-guard")
TRAIN_N = 610


def die(msg):
    sys.exit("guide_jobs: " + msg)


def main():
    if len(sys.argv) < 8:
        die("usage: guide_jobs.py matrix.tsv problems.tsv dev_t.tsv xmatrix.tsv "
            "xproblems.tsv lanes out_dir [picks] [slots]")
    mpath, ppath, dpath, xmpath, xppath, lanes_s, out = sys.argv[1:8]
    picks = int(sys.argv[8]) if len(sys.argv) > 8 else 1000
    n_slots = int(sys.argv[9]) if len(sys.argv) > 9 else 2
    lanes = lanes_s.split(",")
    probs = {}
    j13_stems = set()
    for line in open(ppath):
        f = line.rstrip("\n").split("\t")
        if f[2] == "j13":
            j13_stems.add(stem(f[0]))
        else:
            probs[f[0]] = (f[1], f[2])
    dev_t = set()
    for line in open(dpath):
        f = line.rstrip("\n").split("\t")
        if f[0] != "problem" and f[1] == "train":
            dev_t.add(f[0])
    train = sorted(n for n, (_p, s) in probs.items()
                   if s == "train" and stem(n) not in j13_stems and n not in dev_t)
    if len(train) != TRAIN_N:
        die(f"TRAIN has {len(train)} problems, the registry's {TRAIN_N}")
    xprobs = {}
    for line in open(xppath):
        f = line.rstrip("\n").split("\t")
        if f[2] == "TRAIN-X":
            xprobs[f[0]] = f[1]
    for n in xprobs:
        if x_stem(n) in j13_stems:
            die(f"{n}: a J13 stem in TRAIN-X")
    rows = {}
    for path, split_ok in ((mpath, set(train)), (xmpath, set(xprobs))):
        for line in open(path):
            f = line.rstrip("\n").split("\t")
            if f[2] == "j13":
                die(f"{path} holds j13 rows; pass the sealed working copy")
            if f[0] in split_ok and f[1] in lanes:
                rows[(f[0], f[1])] = f
    jobs = []
    counts = {}
    for name in train + sorted(xprobs):
        split = "train" if name in probs else "TRAIN-X"
        path = probs[name][0] if split == "train" else xprobs[name]
        for lane in lanes:
            r = rows.get((name, lane))
            if r is None:
                die(f"no matrix row for {name} {lane}")
            cls = r[8]
            counts[(lane, split, cls)] = counts.get((lane, split, cls), 0) + 1
            if cls not in SEARCHED:
                continue
            steps = int(r[9]) if r[9].isdigit() else 0
            every = max(1, math.ceil(steps / picks))
            jobs.append((name, path, split, lane, every))
    os.makedirs(out, exist_ok=True)
    order = sorted({(j[2], j[0]) for j in jobs})
    slot_of = {nm: i % n_slots for i, (_s, nm) in enumerate(order)}
    key = lambda j: hashlib.sha256(f"{j[0]}\t{j[3]}".encode()).hexdigest()  # noqa: E731
    with open(os.path.join(out, "jobs.tsv"), "w") as f:
        for j in sorted(jobs, key=key):
            f.write("\t".join(map(str, j)) + "\n")
    for k in range(n_slots):
        with open(os.path.join(out, f"slot_{k + 1}.tsv"), "w") as f:
            for j in sorted((j for j in jobs if slot_of[j[0]] == k), key=key):
                f.write("\t".join(map(str, j)) + "\n")
    with open(os.path.join(out, "problems.txt"), "w") as f:
        for p in sorted({j[1] for j in jobs}):
            f.write(p + "\n")
    lines = [f"guide_jobs: {len(jobs)} jobs on {len(order)} problems, lanes {lanes_s}, picks {picks}, "
             f"{n_slots} slots; TRAIN {len(train)}, TRAIN-X {len(xprobs)}"]
    for lane in lanes:
        for split in ("train", "TRAIN-X"):
            c = {cls: n for (ln, sp, cls), n in counts.items() if ln == lane and sp == split}
            lines.append(f"  {lane} {split}: " + " ".join(f"{k}={v}" for k, v in sorted(c.items())) +
                         f"; jobs {sum(v for k, v in c.items() if k in SEARCHED)}")
    for k in range(n_slots):
        lines.append(f"  slot_{k + 1}: {sum(1 for j in jobs if slot_of[j[0]] == k)} jobs")
    txt = "\n".join(lines)
    with open(os.path.join(out, "summary.txt"), "w") as f:
        f.write(txt + "\n")
    print(txt)


if __name__ == "__main__":
    main()
