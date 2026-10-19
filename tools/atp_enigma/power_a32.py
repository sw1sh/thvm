#!/usr/bin/env python3
"""power_a32.py MATRIX_DIR: the power of M3's online gate G3.2 on DEV-T + DEV-H under a selection
budget (plan amendment A32, after G2.5 failed).  Text processing of the sealed lane matrix only.

Model.  A family's base lane is deterministic: within a budget of B selections it proves a problem
iff its 60 s matrix row proved it with s <= B selections.  The guided twin needs s * r * exp(sigma Z)
(Z iid N(0, 1)); rows the base did not prove are unsolved by both, a lower bound on power.
B is the median of the base lane's selections over its DEV-T + DEV-H proofs.
  NET  on DEV-T + DEV-H: net = gains - losses >= +5 and a one-sided sign test p < 0.05.
  GEO  on TRAIN OOF + DEV-T + DEV-H common solves with s >= 1000: geomean ratio <= 0.75 and its
       bootstrap 95% upper bound < 0.90.
  K3b  P(net <= 0 in every family), families independent.
"""
import collections, csv, math, random, statistics, sys

FAMS = ['tgt-sel', 'encompass', 'twee-deep', 'unfold-tgt-awr']
M = sys.argv[1].rstrip('/') + '/'
devt = {r['problem'] for r in csv.DictReader(open(M + 'dev_t.tsv'), delimiter='\t')}
dev = collections.defaultdict(list)
long_train = collections.defaultdict(list)
for row in csv.reader(open(M + 'matrix.tsv'), delimiter='\t'):
    if len(row) < 10 or row[1] not in FAMS:
        continue
    p, lane, split, cls, steps = row[0], row[1], row[2], row[8], row[9]
    proved = cls == 'proved' and steps.isdigit()
    if split == 'heldout' or p in devt:
        if proved:
            dev[lane].append(int(steps))
    elif split == 'train' and proved and int(steps) >= 1000:
        long_train[lane].append(int(steps))

def sign_p(g, l):
    n = g + l
    return 1.0 if n == 0 else sum(math.comb(n, k) for k in range(g, n + 1)) / 2 ** n

def boot_ub(logs, rng, b=300):
    means = sorted(statistics.fmean(rng.choices(logs, k=len(logs))) for _ in range(b))
    return math.exp(means[int(0.95 * b)])

rng = random.Random(20261019)
SIMS = 1000
kill = collections.defaultdict(lambda: 1.0)
print("family\tB\twindow\tr\tsigma\tP(NET)\tP(net<=0)\tP(GEO)\tGEO n")
for f in FAMS:
    s = sorted(dev[f])
    B = s[(len(s) - 1) // 2]
    window = sum(1 for v in s if B < v <= B / 0.75)
    longs = long_train[f] + [v for v in s if v >= 1000]
    for r in (1.0, 0.75, 0.6, 0.5):
        for sigma in (0.5, 1.0):
            npass = nle0 = gpass = 0
            for _ in range(SIMS):
                g = l = 0
                for sp in s:
                    sg = sp * r * math.exp(sigma * rng.gauss(0, 1))
                    if sp > B and sg <= B:
                        g += 1
                    elif sp <= B and sg > B:
                        l += 1
                npass += g - l >= 5 and sign_p(g, l) < 0.05
                nle0 += g - l <= 0
            for _ in range(300):
                logs = [math.log(r) + sigma * rng.gauss(0, 1) for _ in longs]
                gpass += math.exp(statistics.fmean(logs)) <= 0.75 and boot_ub(logs, rng) < 0.90
            kill[(r, sigma)] *= nle0 / SIMS
            print(f"{f}\t{B}\t{window}\t{r}\t{sigma}\t{npass / SIMS:.2f}\t{nle0 / SIMS:.2f}\t{gpass / 300:.2f}\t{len(longs)}")
for (r, sigma), v in sorted(kill.items()):
    print(f"K3b\tr={r}\tsigma={sigma}\tP(net<=0 in every family)={v:.4f}")
