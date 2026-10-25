#!/usr/bin/env python3
"""ENIGMA comparison statistics.

The analysis of docs/ATP/enigma_portfolio.md section 1.5, the delta of
section 1.2, the J13 seal of section 1.4, gates G0.2 and G0.3, and the
readings registered in section 5.4, over the rows run_arms.py writes.
Python 3.8 or later, standard library only; it processes text and runs
nothing.

Subcommands:
  compare  paired comparisons of arms at each wall
  delta    the A/A disagreement D of two passes of one arm, delta, gate G0.2
  wdep     gate G0.3: the submission build's runs against the official J13
           result, and WDEP

Definitions (fixed here, before the data):
- A run is a Solution when run_arms.py's solution column is 1: SZS status
  Unsatisfiable, a complete CNFRefutation block, the run ended within the
  wall, strict CheckProof VerifiedGood.  An SZS solve is status
  Unsatisfiable within the wall, proof or not.
- A cell is one (problem, arm, wall).  Its outcome is the strict majority
  of its passes (the majority of three when the repeat rule fired; the one
  pass otherwise); its time is the smallest t within which a strict
  majority of its passes found a Solution (inf when unsolved).  "ARM@K"
  names pass K of ARM alone (the A/A passes of L0).
- A cell is complete when its pass set is the one the runner's
  decisions.tsv requires ({1} or {1, 2, 3}), or, without decisions, the
  union of the passes the other arms ran there.  At each wall every
  comparison uses one problem set: the problems complete in every arm
  named at that wall.  At looks L0, L1 and L2 a problem incomplete in any
  named arm (or, for J13, fewer than the 400 problems) makes the wall
  INVALID: nothing is compared there and the exit code is 3; the runner
  must resume until the cells are complete.  In dev analyses such a
  problem is compared on pass 1 of every arm (when every arm has one) and
  counted on the FALLBACK line.
- net(A - B) = gains - losses over the compared problems, a gain being a
  problem A solves and B does not.
- Sign test: exact binomial on the discordant problems, p = 1/2.  The
  one-sided p is P(X >= gains); the two-sided p is twice the smaller tail,
  capped at 1.  Gates use the one-sided p (gate_p): a DEV gate "net >= +k"
  also needs gate_p < 0.05, and so does G3.3's "McNemar p < 0.05".
- Bootstrap CI: problems clustered by name stem (the domain letters and the
  number, e.g. COL042 for COL042-6), stems resampled with replacement,
  10,000 resamples, seed 20261001 (Mersenne Twister, indices by rejection
  from getrandbits), percentile interval: the ceil(a B)-th and
  ceil((1 - a) B)-th smallest of the B resampled nets, a = (1 - level) / 2.
  Levels 95% and 97.5% (claim A's per-wall test).  At L0-L2 another seed
  or count needs --amendment naming a section 5.4 entry.
- Domains: per TPTP domain (leading letters of the name) the gains,
  losses and net.  At L0 and L1 every domain with fewer than 10 compared
  problems is pooled into OTHER (and while OTHER holds 1-9 problems, the
  smallest named domain joins it); the unpooled table is sealed.  The
  harm reading of M5's "no TPTP domain loses more than 3 problems" is a
  domain's (or OTHER's) loss count.
- Best domain: the domain (OTHER included when pooled) with the largest
  net (ties: alphabetical first); the drop-best net is the net without it.
- Near-wall band: an arm's Solutions with time in (W/2, W]; and its misses
  at W that are Solutions in a run of the same arm at 2W, when rows at 2W
  exist.
- Unknown: a cell that is not a Solution and whose most frequent outcome
  among its non-Solution passes is "Unsatisfiable, proof within the wall,
  verdict neither VerifiedGood nor VerifiedBad"; a tie with another
  category goes to Unknown (conservative toward INCONCLUSIVE) and is
  counted as tie_decided.  The lane of an Unknown is that pass's winning
  lane.  If two arms' Unknown counts differ by more than delta, their
  comparison is INCONCLUSIVE.  At L0 and L1 Unknowns are printed per arm;
  the per-lane counts are sealed.
- delta = min(4, max(2, ceil(sqrt(D)))), D = problems where the two A/A
  passes disagree on Solution at that wall.  G0.2: D <= 12 at each wall,
  with both passes present on every expected problem (400 for J13).
- NL at a wall: of the --nl arms, the one with the most Solutions on the
  wall's problem set, chosen once per wall; a tie goes to the arm that
  gives the first comparison naming NL the lowest 95% CI lower bound (then
  the higher one-sided p, then list order).
- J13 subsets (when every compared problem is a j13 row): casc30-recurring
  (in the CASC-30 UEQ list), casc28-only (in the CASC-28 list, not
  CASC-30), new (in neither); minus10 (version -10, size suffix allowed)
  and other; axiom-novel (--axiom-novel file); residue classes (--residue
  file: name TAB class).
- Miss end states: the runner's end class of the cell's most frequent
  non-Solution end; a miss whose search proved is counted by why it is not
  a Solution instead (unknown, verified-bad, no-proof).
- G0.3 (wdep): a pod run solved a problem when it printed SZS status
  Unsatisfiable and a complete CNFRefutation block within the wall (CASC's
  "+" marks a solution output; blocks holding no record are counted apart
  as empty_blocks).  Agreement is the share of the official problems whose
  solved flag matches the official "+".  A wall qualifies with every
  problem run, agreement >= 0.95 and a total within 8 of the official
  count; WDEP is the qualifying wall with the highest agreement (ties: the
  total closer to the official count, then the lower wall).  Otherwise
  WDEP = 180 / c, c the geometric mean of official / pod time over the
  problems solved in both with official time >= 1 s in the largest wall's
  run, labelled uncalibrated.  Rows run under the 3500 MB cap cannot
  calibrate: they are refused, or (--allow-capped) the result is labelled
  uncalibrated.
- Validity, printed in every mode: pod types, run conditions, binaries per
  arm, runs past W and past W + 10 s, problems whose runs span several
  pods, OOM kills, complete and incomplete cells, VerifiedBad runs.  Rows
  of one wall with different rss caps are INVALID (exit 3).  Any
  VerifiedBad stops the line (exit 4): at L0-L2 nothing is compared, in
  dev analyses every comparison line is tagged VOID.

The seal (section 1.4): rows with split j13 are read only at a look.
--look L0 and L1 print only the pre-registered aggregates (totals, nets
and CIs per arm and wall, discordant counts and sign tests, pooled domain
counts, band counts, Unknown counts per arm, delta and the inconclusive
rule) and write every per-problem table, the end states, the per-lane
Unknowns, the unpooled domains and the J13 subsets into a read-only gzip
under ~/.cache/thvm/enigma/sealed/ without printing them.  --look L2 needs
--l2-token equal to the "L2 unseal token" entered in the plan's section
5.4 registry after the freeze.  delta and wdep need --look L0 for j13
rows.  Without a look, a j13 row is an error: every other analysis reads
no J13 row.
"""
import argparse
import collections
import fractions
import gzip
import hashlib
import io
import math
import os
import random
import re
import sys
import tarfile
import time

INF = float("inf")
LEVELS = (0.95, 0.975)
DEFAULT_SEED = 20261001
DEFAULT_RESAMPLES = 10000
G02_MAX_D = 12
G03_MIN_AGREEMENT = 0.95
G03_MAX_TOTAL_DIFF = 8
J13_PROBLEMS = 400
DOMAIN_POOL_MIN = 10
HERE = os.path.dirname(os.path.abspath(__file__))
PLAN = os.path.normpath(os.path.join(HERE, "..", "..", "..", "docs", "plans",
                                     "enigma_portfolio.md"))
J13_DIR = os.path.expanduser("~/.cache/thvm/casc/J13")
SEALED_DIR = os.path.expanduser("~/.cache/thvm/enigma/sealed")
CATEGORY_ORDER = ("unknown", "verified-bad", "no-proof", "unchecked", "miss")
MISS_CLASSES = ("memory-guard", "wall", "saturated", "immediate-exit", "alarm",
                "step-cap", "crash", "unknown")
LOOKS = ("dev", "L0", "L1", "L2")
EXIT_INVALID = 3
EXIT_VBAD = 4


# ----------------------------------------------------------------------------
# Names


def fmt_num(x):
    x = float(x)
    if x == int(x):
        return str(int(x))
    return ("%.3f" % x).rstrip("0").rstrip(".")


def stem_of(name):
    m = re.match(r"^([A-Z]{3}\d{3})", name)
    if m:
        return m.group(1)
    return re.split(r"[-+^=_.]", name, maxsplit=1)[0]


def domain_of(name):
    m = re.match(r"[A-Z]+", name)
    return m.group(0) if m else name


def is_minus10(name):
    return re.match(r"^[A-Z]{3}\d{3}-10(\.\d+)?$", name) is not None


def cond_field(cond, key):
    for part in cond.split(";"):
        k, eq, v = part.partition("=")
        if eq and k == key:
            return v
    return ""


# ----------------------------------------------------------------------------
# Loading run_arms output


class Run(object):
    __slots__ = ("problem", "split", "arm", "wall", "pass_no", "status", "wall_s", "lane",
                 "proof", "verdict", "solution", "lscpu", "bin_sha", "in_wall", "end_class",
                 "lane_classes", "host", "cond", "proof_recs", "oom_kills")

    def __init__(self, d):
        self.problem = d["problem"]
        self.split = d["split"]
        self.arm = d["arm"]
        self.wall = fmt_num(d["wall"])
        self.pass_no = int(d["pass"])
        self.status = d["status"]
        self.wall_s = float(d["wall_s"])
        self.lane = d.get("lane", "")
        self.proof = int(d.get("proof", "0") or 0)
        self.verdict = d.get("verdict", "-")
        self.solution = int(d["solution"])
        self.lscpu = d.get("lscpu", "")
        self.bin_sha = d.get("bin_sha", "")
        self.in_wall = int(d.get("in_wall", "1") or 0)
        self.end_class = d.get("end_class", "")
        self.lane_classes = d.get("lane_classes", "")
        self.host = d.get("host", "")
        self.cond = d.get("cond", "")
        self.proof_recs = int(d.get("proof_recs", "-1") or -1)
        self.oom_kills = d.get("oom_kills", "?")


MALFORMED = {"rows": 0}


def parse_tsv_text(text):
    """Rows under the '#' header line.  A line with the wrong field count (a
    row cut by a crash, which the runner re-ran) is skipped and counted."""
    header = None
    out = []
    for line in text.splitlines():
        if not line.strip():
            continue
        if line.startswith("#"):
            if header is None:
                header = line[1:].split("\t")
            continue
        if header is None:
            raise SystemExit("stats: a TSV without its '#' header line")
        f = line.split("\t")
        if len(f) != len(header):
            MALFORMED["rows"] += 1
            continue
        out.append(dict(zip(header, f)))
    return out


def _texts_from_tar(tf):
    """rows.tsv and decisions.tsv members, also inside a nested sealed.tgz
    (a sealed run directory packs them there)."""
    texts = []
    for m in tf.getmembers():
        if not m.isfile():
            continue
        base = os.path.basename(m.name)
        if base in ("rows.tsv", "decisions.tsv"):
            fp = tf.extractfile(m)
            texts.append((base, fp.read().decode("utf-8", "replace") if fp else ""))
        elif base == "sealed.tgz":
            fp = tf.extractfile(m)
            if fp is not None:
                with tarfile.open(fileobj=io.BytesIO(fp.read()), mode="r:*") as inner:
                    texts.extend(_texts_from_tar(inner))
    return texts


def load_runs(paths):
    """(runs, decisions) from rows.tsv files, run directories (plain or
    sealed), or tar archives holding them.  decisions: (problem, wall) ->
    repeat flag."""
    texts = []
    for path in paths:
        path = os.path.expanduser(path)
        if os.path.isdir(path):
            for name in ("rows.tsv", "decisions.tsv"):
                f = os.path.join(path, name)
                if os.path.exists(f):
                    with open(f) as fp:
                        texts.append((name, fp.read()))
            sealed = os.path.join(path, "sealed.tgz")
            if os.path.exists(sealed):
                with tarfile.open(sealed, "r:*") as tf:
                    texts.extend(_texts_from_tar(tf))
        elif tarfile.is_tarfile(path):
            with tarfile.open(path, "r:*") as tf:
                texts.extend(_texts_from_tar(tf))
        else:
            opener = gzip.open if path.endswith(".gz") else open
            with opener(path, "rt") as fp:
                texts.append(("rows.tsv", fp.read()))
    runs = []
    decisions = {}
    for kind, text in texts:
        if kind == "decisions.tsv":
            for d in parse_tsv_text(text):
                decisions[(d["problem"], fmt_num(d["wall"]))] = int(d["repeat"])
        else:
            runs.extend(Run(d) for d in parse_tsv_text(text))
    seen = set()
    dup = 0
    for r in runs:
        k = (r.problem, r.arm, r.wall, r.pass_no)
        if k in seen:
            dup += 1
        seen.add(k)
    if dup:
        raise SystemExit("stats: %d duplicate (problem, arm, wall, pass) rows across the inputs" % dup)
    return runs, decisions


def read_names(path):
    names = []
    with open(os.path.expanduser(path)) as fp:
        for line in fp:
            line = line.strip()
            if line and not line.startswith("#"):
                names.append(line.split("\t")[0].split()[0])
    return names


def read_classes(path):
    out = {}
    with open(os.path.expanduser(path)) as fp:
        for line in fp:
            line = line.rstrip("\n")
            if not line.strip() or line.startswith("#"):
                continue
            f = line.split("\t")
            if len(f) < 2:
                raise SystemExit("stats: %s: need name TAB class" % path)
            out[f[0].strip()] = f[1].strip()
    return out


# ----------------------------------------------------------------------------
# Looks and the plan's registry


def plan_registry(plan_path):
    """(registry rows {item: value}, section 5.4 text) of the plan."""
    try:
        with open(plan_path) as fp:
            text = fp.read()
    except OSError:
        raise SystemExit("stats: cannot read the plan %s" % plan_path)
    m = re.search(r"^### 5\.4 .*?(?=^## |\Z)", text, re.M | re.S)
    sec = m.group(0) if m else ""
    rows = {}
    for line in sec.splitlines():
        f = [x.strip() for x in line.strip().strip("|").split("|")]
        if len(f) >= 2 and line.lstrip().startswith("|"):
            rows[f[0]] = f[1]
    return rows, sec


def check_look(args, has_j13, what):
    """Refuse a look the seal does not allow; return the look."""
    look = args.look or "dev"
    if has_j13 and look == "dev":
        raise SystemExit("stats: %s: J13 rows are sealed; give the look (--look L0, L1 or L2)"
                         % what)
    if look == "L2":
        rows, _sec = plan_registry(args.plan)
        token = rows.get("L2 unseal token", "")
        if not args.l2_token or not re.match(r"^[0-9a-f]{12,64}$", token) \
                or args.l2_token != token:
            raise SystemExit("stats: --look L2 needs --l2-token equal to the 'L2 unseal token' "
                             "row of %s section 5.4 (entered after the freeze; now %r)"
                             % (args.plan, token))
    return look


def check_conditions(args, look):
    """Another seed or resample count at a look needs a registered amendment."""
    if look == "dev":
        return
    if args.seed == DEFAULT_SEED and args.resamples == DEFAULT_RESAMPLES:
        return
    _rows, sec = plan_registry(args.plan)
    if not args.amendment or args.amendment not in sec:
        raise SystemExit("stats: --seed %d --resamples %d at look %s: the plan registers %d and "
                         "%d; give --amendment TAG naming a section 5.4 entry" % (
                             args.seed, args.resamples, look, DEFAULT_SEED, DEFAULT_RESAMPLES))


# ----------------------------------------------------------------------------
# Cells: the majority rule


def pass_category(r):
    if r.solution:
        return "solution"
    if r.verdict == "VerifiedBad":
        return "verified-bad"
    if r.status == "Unsatisfiable" and r.proof and r.in_wall:
        return "unchecked" if r.verdict in ("-", "") else "unknown"
    if r.status == "Unsatisfiable":
        return "no-proof"
    return "miss"


class Cell(object):
    __slots__ = ("problem", "arm", "wall", "runs", "sol", "szs", "time", "lane",
                 "category", "end_class", "lane_classes", "unknown_lane", "split",
                 "unknown_tie")

    def __init__(self, runs):
        runs = sorted(runs, key=lambda r: r.pass_no)
        self.runs = runs
        self.problem = runs[0].problem
        self.arm = runs[0].arm
        self.wall = runs[0].wall
        self.split = runs[0].split
        n = len(runs)
        k = sum(r.solution for r in runs)
        self.sol = int(2 * k > n)
        self.szs = int(2 * sum(1 for r in runs if r.status == "Unsatisfiable" and r.in_wall) > n)
        times = sorted(r.wall_s if r.solution else INF for r in runs)
        self.time = times[n // 2]
        self.lane = ""
        self.category = "solution" if self.sol else ""
        self.end_class = "proved" if self.sol else ""
        self.lane_classes = ""
        self.unknown_lane = ""
        self.unknown_tie = False
        if self.sol:
            for r in runs:
                if r.solution and r.wall_s == self.time:
                    self.lane = r.lane
                    break
            return
        rest = [r for r in runs if not r.solution]
        cats = collections.Counter(pass_category(r) for r in rest)
        top = max(cats.values())
        for c in CATEGORY_ORDER:
            if cats.get(c) == top:
                self.category = c
                break
        self.unknown_tie = (self.category == "unknown"
                            and sum(1 for v in cats.values() if v == top) > 1)
        ends = collections.Counter(r.end_class for r in rest)
        etop = max(ends.values())
        for r in rest:
            if ends[r.end_class] == etop:
                self.end_class = r.end_class
                self.lane_classes = r.lane_classes
                break
        for r in rest:
            if pass_category(r) == self.category:
                self.lane = r.lane
                if self.category == "unknown":
                    self.unknown_lane = r.lane or "-"
                break


def parse_arm_spec(spec):
    arm, at, p = spec.partition("@")
    return arm, (int(p) if at else None)


def build_cells(runs, decisions, wall, arm_specs):
    """(cells, incomplete, first) at `wall`: cells[spec][problem] the
    complete cells, incomplete[spec] the problems whose pass set is not
    the required one, first[spec][problem] the pass-1 cell (the dev
    fallback)."""
    by = collections.defaultdict(list)
    for r in runs:
        if r.wall == wall:
            by[(r.arm, r.problem)].append(r)
    passes_at = collections.defaultdict(set)
    for (arm, prob), rs in by.items():
        for r in rs:
            passes_at[prob].add(r.pass_no)
    cells = {}
    incomplete = {}
    first = {}
    for spec in arm_specs:
        arm, only = parse_arm_spec(spec)
        cells[spec] = {}
        incomplete[spec] = set()
        first[spec] = {}
        for (a, prob), rs in by.items():
            if a != arm:
                continue
            if only is not None:
                sel = [r for r in rs if r.pass_no == only]
                if sel:
                    cells[spec][prob] = Cell(sel)
                    first[spec][prob] = cells[spec][prob]
                else:
                    incomplete[spec].add(prob)
                continue
            p1 = [r for r in rs if r.pass_no == 1]
            if p1:
                first[spec][prob] = Cell(p1)
            have = {r.pass_no for r in rs}
            if (prob, wall) in decisions:
                want = {1, 2, 3} if decisions[(prob, wall)] else {1}
            else:
                want = passes_at[prob]
            if have != want:
                incomplete[spec].add(prob)
                continue
            cells[spec][prob] = Cell(rs)
    return cells, incomplete, first


# ----------------------------------------------------------------------------
# Statistics


def binom_tail_ge(k, n):
    """P(X >= k) for X ~ Binomial(n, 1/2), exactly (no float overflow)."""
    if k <= 0:
        return 1.0
    if k > n:
        return 0.0
    return float(fractions.Fraction(sum(math.comb(n, i) for i in range(k, n + 1)), 1 << n))


def sign_test(gains, losses):
    """(one-sided p for 'A better', two-sided p)."""
    n = gains + losses
    if n == 0:
        return 1.0, 1.0
    p1 = binom_tail_ge(gains, n)
    p2 = min(1.0, 2.0 * min(binom_tail_ge(gains, n), binom_tail_ge(losses, n)))
    return p1, p2


def delta_from_d(d):
    return min(4, max(2, int(math.ceil(math.sqrt(d)))))


def _below(rng, n):
    k = n.bit_length()
    while True:
        r = rng.getrandbits(k)
        if r < n:
            return r


def bootstrap_nets(diffs, stems, resamples=DEFAULT_RESAMPLES, seed=DEFAULT_SEED):
    """Sorted resampled nets: stems drawn with replacement, each carrying the
    summed differences of its problems."""
    sums = collections.defaultdict(int)
    for p, dv in diffs.items():
        sums[stems[p]] += dv
    keys = sorted(sums)
    vals = [sums[k] for k in keys]
    n = len(vals)
    if n == 0:
        return [0] * resamples
    rng = random.Random(seed)
    nets = []
    for _ in range(resamples):
        s = 0
        for _j in range(n):
            s += vals[_below(rng, n)]
        nets.append(s)
    nets.sort()
    return nets


def percentile_ci(sorted_nets, level):
    b = len(sorted_nets)
    a = (1.0 - level) / 2.0
    lo = sorted_nets[max(0, int(math.ceil(a * b - 1e-9)) - 1)]
    hi = sorted_nets[min(b - 1, int(math.ceil((1.0 - a) * b - 1e-9)) - 1)]
    return lo, hi


def cluster_ci(diffs, stems, levels=LEVELS, resamples=DEFAULT_RESAMPLES, seed=DEFAULT_SEED):
    nets = bootstrap_nets(diffs, stems, resamples, seed)
    return {lev: percentile_ci(nets, lev) for lev in levels}


def pool_domains(domains, k=DOMAIN_POOL_MIN):
    """Domains with fewer than k problems pooled into OTHER; while OTHER
    holds 1..k-1 problems the smallest named domain joins it."""
    named = {d: dict(v) for d, v in domains.items() if v["n"] >= k}
    other = {"net": 0, "n": 0, "gains": 0, "losses": 0}
    for d, v in domains.items():
        if v["n"] < k:
            for f in other:
                other[f] += v[f]
    while 0 < other["n"] < k and named:
        d = sorted(named, key=lambda x: (named[x]["n"], x))[0]
        for f in other:
            other[f] += named[d][f]
        del named[d]
    out = collections.OrderedDict((d, named[d]) for d in sorted(named))
    if other["n"]:
        out["OTHER"] = other
    return out


def drop_best(domains, net):
    if not domains:
        return "-", 0
    best = sorted(domains, key=lambda d: (-domains[d]["net"], d))[0]
    return best, net - domains[best]["net"]


def compare_cells(ca, cb, problems, wall, two_w=None, levels=LEVELS,
                  resamples=DEFAULT_RESAMPLES, seed=DEFAULT_SEED, with_ci=True):
    """Paired comparison of two arms' cells on `problems` at `wall`.
    two_w: (cells of A at 2W, cells of B at 2W) or None."""
    w = float(wall)
    diffs = {p: ca[p].sol - cb[p].sol for p in problems}
    stems = {p: stem_of(p) for p in problems}
    gains = sum(1 for v in diffs.values() if v > 0)
    losses = sum(1 for v in diffs.values() if v < 0)
    net = gains - losses
    p1, p2 = sign_test(gains, losses)
    res = {"n": len(problems), "gains": gains, "losses": losses, "net": net,
           "p_one": p1, "p_two": p2,
           "total_a": sum(ca[p].sol for p in problems),
           "total_b": sum(cb[p].sol for p in problems),
           "szs_a": sum(ca[p].szs for p in problems),
           "szs_b": sum(cb[p].szs for p in problems)}
    res["ci"] = cluster_ci(diffs, stems, levels, resamples, seed) if with_ci else {}
    dom = collections.OrderedDict()
    for p in sorted(problems, key=lambda x: (domain_of(x), x)):
        v = dom.setdefault(domain_of(p), {"net": 0, "n": 0, "gains": 0, "losses": 0})
        v["net"] += diffs[p]
        v["n"] += 1
        v["gains"] += int(diffs[p] > 0)
        v["losses"] += int(diffs[p] < 0)
    res["domains"] = dom
    res["best_domain"], res["net_drop_best"] = drop_best(dom, net)
    for side, cells in (("a", ca), ("b", cb)):
        band = sum(1 for p in problems if cells[p].sol and w / 2.0 < cells[p].time <= w)
        res["band_" + side] = band
        unknown = [p for p in problems if cells[p].category == "unknown"]
        res["unknown_" + side] = len(unknown)
        res["unknown_tie_" + side] = sum(1 for p in unknown if cells[p].unknown_tie)
        res["unknown_lanes_" + side] = collections.Counter(cells[p].unknown_lane for p in unknown)
        # A miss whose search proved (checker Unknown, no proof block, proof
        # past the wall) is reported by that category, not as "proved".
        res["ends_" + side] = collections.Counter(
            (cells[p].end_class or "unknown") if cells[p].category == "miss"
            else cells[p].category for p in problems if not cells[p].sol)
        res["vbad_" + side] = sum(1 for p in problems for r in cells[p].runs
                                  if r.verdict == "VerifiedBad")
    if two_w is not None:
        for side, cells, cells2 in (("a", ca, two_w[0]), ("b", cb, two_w[1])):
            if cells2:
                misses = [p for p in problems if not cells[p].sol and p in cells2]
                res["band2w_" + side] = sum(1 for p in misses if cells2[p].sol)
                res["band2w_n_" + side] = len(misses)
    table = []
    for p in sorted(problems):
        if diffs[p] == 0:
            continue
        loser = cb[p] if diffs[p] > 0 else ca[p]
        table.append([p, domain_of(p), stem_of(p), "A" if diffs[p] > 0 else "B",
                      _t(ca[p]), ca[p].lane or "-", ca[p].category,
                      _t(cb[p]), cb[p].lane or "-", cb[p].category,
                      loser.end_class or "-", loser.lane_classes or "-"])
    res["table"] = table
    return res


def _t(cell):
    return "%.2f" % cell.time if cell.time != INF else "-"


def choose_nl(nl_arms, cells, problems, other, wall, resamples, seed):
    """The NL arm of a wall (see the module docstring); `other` is the arm
    of the first comparison naming NL, which breaks a tie."""
    totals = {a: sum(cells[a][p].sol for p in problems) for a in nl_arms}
    best = max(totals.values())
    tied = [a for a in nl_arms if totals[a] == best]
    if len(tied) == 1 or other is None:
        return tied[0], tied, totals
    scored = []
    for i, a in enumerate(tied):
        r = compare_cells(cells[other], cells[a], problems, wall, resamples=resamples, seed=seed)
        scored.append((r["ci"][0.95][0], -r["p_one"], i, a))
    scored.sort()
    return scored[0][3], tied, totals


def j13_subsets(problems, casc30, casc28, axiom_novel=None, residue=None):
    """name -> set of problems for the section 1.5 J13 subsets."""
    ps = set(problems)
    c30 = set(casc30)
    c28 = set(casc28)
    subs = collections.OrderedDict()
    subs["casc30-recurring"] = {p for p in ps if p in c30}
    subs["casc28-only"] = {p for p in ps if p in c28 and p not in c30}
    subs["new"] = {p for p in ps if p not in c30 and p not in c28}
    subs["minus10"] = {p for p in ps if is_minus10(p)}
    subs["other-than-minus10"] = {p for p in ps if not is_minus10(p)}
    if axiom_novel is not None:
        an = set(axiom_novel)
        subs["axiom-novel"] = {p for p in ps if p in an}
    if residue is not None:
        for cls in sorted(set(residue.values())):
            subs["residue:" + cls] = {p for p in ps if residue.get(p) == cls}
    return subs


J13_SUBSET_SIZES = {"casc30-recurring": 232, "casc28-only": 59, "new": 109,
                    "minus10": 116, "other-than-minus10": 284}


# ----------------------------------------------------------------------------
# The seal


def write_sealed(sealed_dir, label, sections):
    """Write the per-problem sections into a new read-only gzip; return
    (path, sha256).  Nothing of it is printed."""
    sealed_dir = os.path.expanduser(sealed_dir)
    os.makedirs(sealed_dir, mode=0o700, exist_ok=True)
    if not re.match(r"^[A-Za-z0-9_.+-]+$", label):
        raise SystemExit("stats: --label: use letters, digits, _ . + -")
    stamp = time.strftime("%Y%m%dT%H%M%SZ", time.gmtime())
    path = os.path.join(sealed_dir, "%s.%s.tsv.gz" % (label, stamp))
    if os.path.exists(path):
        raise SystemExit("stats: %s exists; refusing to overwrite a sealed file" % path)
    buf = io.StringIO()
    for title, header, rows in sections:
        buf.write("## %s\n" % title)
        buf.write("#" + "\t".join(header) + "\n")
        for row in rows:
            buf.write("\t".join(str(x) for x in row) + "\n")
    data = buf.getvalue().encode()
    with open(path, "wb") as raw:
        with gzip.GzipFile(filename="", mode="wb", fileobj=raw, mtime=0) as gz:
            gz.write(data)
    os.chmod(path, 0o400)
    h = hashlib.sha256()
    with open(path, "rb") as fp:
        h.update(fp.read())
    return path, h.hexdigest()


# ----------------------------------------------------------------------------
# compare


def _fmt_p(p):
    return "%.4g" % p


def _ci_text(ci):
    out = []
    for lev in LEVELS:
        if lev in ci:
            lo, hi = ci[lev]
            out.append("ci%s=[%+d,%+d]" % (fmt_num(lev * 100), lo, hi))
    return " ".join(out)


def _dom_text(domains):
    return " ".join("%s:%+d/%d(+%d,-%d)" % (d, v["net"], v["n"], v["gains"], v["losses"])
                    for d, v in domains.items())


def delta_by_wall(runs, arm, walls, expect=None):
    """wall -> (D, delta, n) from the A/A passes 1 and 2 of `arm`; refuses
    a wall where a problem lacks one of the passes, or with fewer than
    `expect` problems."""
    out = {}
    for w in walls:
        p1 = {r.problem: r.solution for r in runs if r.arm == arm and r.wall == w and r.pass_no == 1}
        p2 = {r.problem: r.solution for r in runs if r.arm == arm and r.wall == w and r.pass_no == 2}
        common = set(p1) & set(p2)
        union = set(p1) | set(p2)
        if not union:
            continue
        if common != union or (expect is not None and len(common) != expect):
            raise SystemExit("stats: INVALID A/A rows of %s at wall %s: %d problems with both "
                             "passes of %d with any (expected %s); delta is not set from "
                             "partial data" % (arm, w, len(common), len(union), expect or "all"))
        d = sum(1 for p in common if p1[p] != p2[p])
        out[w] = (d, delta_from_d(d), len(common))
    return out


def parse_delta_args(items):
    """--delta N (every wall) or --delta W=N."""
    out = {}
    for it in items or []:
        if "=" in it:
            w, _, v = it.partition("=")
            out[fmt_num(w)] = int(v)
        else:
            out["*"] = int(it)
    return out


def validity_lines(runs, walls, say):
    lscpu = collections.Counter(r.lscpu for r in runs)
    conds = collections.Counter(r.cond for r in runs)
    for k, v in sorted(lscpu.items()):
        say("POD %s rows=%d" % (k or "-", v))
    for k, v in sorted(conds.items()):
        say("COND %s rows=%d proof=%s" % (k or "-", v, "on" if "proof=1" in k else "?"))
    shas = collections.defaultdict(collections.Counter)
    for r in runs:
        shas[r.arm][r.bin_sha] += 1
    for arm in sorted(shas):
        say("BINARY arm=%s %s%s" % (arm, " ".join("%s:%d" % (s[:12], n) for s, n in
                                                   sorted(shas[arm].items())),
                                    "" if len(shas[arm]) == 1 else " WARNING: several binaries"))
    for w in walls:
        sel = [r for r in runs if r.wall == w]
        over = sum(1 for r in sel if r.wall_s > float(w))
        over10 = sum(1 for r in sel if r.wall_s > float(w) + 10)
        say("OVERRUN wall=%s runs_past_W=%d runs_past_W+10=%d" % (w, over, over10))
        hosts_by_prob = collections.defaultdict(set)
        for r in sel:
            hosts_by_prob[r.problem].add(r.host)
        say("PAIRING wall=%s problems_with_runs_on_several_pods=%d (all arms of a problem "
            "belong on one pod)" % (w, sum(1 for v in hosts_by_prob.values() if len(v) > 1)))
        kills = sum(1 for r in sel if r.oom_kills not in ("?", "", "0"))
        unread = sum(1 for r in sel if r.oom_kills in ("?", ""))
        say("OOM wall=%s runs_with_oom_kills=%d runs_unreadable=%d" % (w, kills, unread))
    if MALFORMED["rows"]:
        say("MALFORMED rows skipped=%d (cut lines; the runner re-ran those jobs)"
            % MALFORMED["rows"])


def cmd_compare(args):
    runs, decisions = load_runs(args.rows)
    if not runs:
        raise SystemExit("stats: no rows")
    has_j13 = any(r.split == "j13" for r in runs)
    look = check_look(args, has_j13, "compare")
    check_conditions(args, look)
    sealed = look in ("L0", "L1")
    strict = look in ("L0", "L1", "L2")
    if sealed and not args.label:
        raise SystemExit("stats: --look %s needs --label (it names the sealed file)" % look)
    expect = args.expect_problems
    if expect is None and has_j13:
        expect = J13_PROBLEMS
    pairs = []
    for spec in args.pair:
        a, _, b = spec.partition(",")
        if not a or not b:
            raise SystemExit("stats: --pair A,B")
        pairs.append((a, b))
    nl_arms = [x for x in (args.nl or "").split(",") if x]
    if any("NL" in pr for pr in pairs) and not nl_arms:
        raise SystemExit("stats: a pair names NL: give --nl P0,P1,PD")
    walls = [fmt_num(w) for w in args.wall] if args.wall else sorted(
        {r.wall for r in runs}, key=float)
    deltas = parse_delta_args(args.delta)
    if args.aa_rows:
        aa_runs, _ = load_runs(args.aa_rows)
        aa_expect = J13_PROBLEMS if any(r.split == "j13" for r in aa_runs) else None
        for w, (d, dl, _n) in delta_by_wall(aa_runs, args.aa_arm, walls, aa_expect).items():
            deltas.setdefault(w, dl)
            deltas.setdefault("D@" + w, d)
    casc30 = casc28 = None
    if os.path.exists(os.path.expanduser(args.casc30)):
        casc30 = read_names(args.casc30)
    if os.path.exists(os.path.expanduser(args.casc28)):
        casc28 = read_names(args.casc28)
    axiom_novel = read_names(args.axiom_novel) if args.axiom_novel else None
    residue = read_classes(args.residue) if args.residue else None

    out = []
    sealed_sections = []
    tag = {"void": ""}

    def say(line):
        out.append(tag["void"] + line)

    rc = 0
    n_vbad = sum(1 for r in runs if r.verdict == "VerifiedBad")
    say("CONDITIONS rows=%d walls=%s hosts=%d count=Solutions(SZS Unsatisfiable + CNFRefutation "
        "within W + strict CheckProof VerifiedGood, strict majority of the passes run) also=SZS "
        "solves(Unsatisfiable within W) look=%s seed=%d resamples=%d%s" % (
            len(runs), ",".join(walls), len({r.host for r in runs}), look, args.seed,
            args.resamples, (" amendment=%s" % args.amendment) if args.amendment else ""))
    validity_lines(runs, walls, say)
    if n_vbad:
        say("STOP-THE-LINE VerifiedBad runs=%d: no result here stands until the cause is fixed"
            % n_vbad)
        rc = EXIT_VBAD
        if strict:
            print("\n".join(out))
            return rc
        tag["void"] = "VOID "

    all_specs = set()
    for a, b in pairs:
        for x in (a, b):
            if x != "NL":
                all_specs.add(x)
    all_specs.update(nl_arms)
    spec_arms = {parse_arm_spec(s)[0] for s in all_specs}
    if strict and has_j13:
        names = {r.problem for r in runs if r.split == "j13" and r.arm in spec_arms}
        if len(names) != expect:
            say("INVALID rows name %d J13 problems for the compared arms; a look reads all %d "
                "together (every shard)" % (len(names), expect))
            print("\n".join(out))
            return EXIT_INVALID
    cell_rows = []
    lane_rows = []
    dom_rows = []
    for w in walls:
        rss = {cond_field(r.cond, "rss") for r in runs if r.wall == w and r.arm in spec_arms}
        conds = {r.cond for r in runs if r.wall == w and r.arm in spec_arms}
        if len(rss) > 1:
            say("INVALID wall=%s rows run under different rss caps (%s); a comparison runs "
                "under one cap" % (w, ",".join(sorted(rss))))
            rc = rc or EXIT_INVALID
            continue
        if len(conds) > 1:
            say("COND-MIX wall=%s %d run conditions among the compared arms (see COND lines)"
                % (w, len(conds)))
        cells, incomplete, first = build_cells(runs, decisions, w, sorted(all_specs))
        w2 = fmt_num(2 * float(w))
        has_2w = any(r.wall == w2 for r in runs)
        cells2 = build_cells(runs, decisions, w2, sorted(all_specs))[0] if has_2w else None
        for spec in sorted(all_specs):
            say("CELLS wall=%s arm=%s complete=%d incomplete=%d" % (
                w, spec, len(cells[spec]), len(incomplete[spec])))
        seen = set()
        for spec in all_specs:
            seen |= set(cells[spec]) | incomplete[spec]
        complete = set.intersection(*[set(cells[s]) for s in all_specs]) if all_specs else set()
        missing = seen - complete
        if strict and (missing or (has_j13 and len(complete) != expect)):
            say("INVALID wall=%s %d problems complete in every arm, %d not%s: nothing is "
                "compared at this look until the runner completes them" % (
                    w, len(complete), len(missing),
                    (" (expected %d)" % expect) if has_j13 else ""))
            rc = rc or EXIT_INVALID
            continue
        if missing:
            fb = {p for p in missing if all(p in first[s] for s in all_specs)}
            for s in all_specs:
                for p in fb:
                    cells[s][p] = first[s][p]
            complete |= fb
            say("FALLBACK wall=%s problems_on_pass_1=%d dropped=%d (dev rule: pass 1 of every "
                "arm where a pass set is incomplete)" % (w, len(fb), len(missing) - len(fb)))
        problems = sorted(complete)
        for spec in sorted(all_specs):
            for p in problems:
                c = cells[spec][p]
                cell_rows.append([w, spec, p, c.split, c.sol, c.szs, _t(c), c.lane or "-",
                                  c.category, c.end_class or "-", c.lane_classes or "-",
                                  len(c.runs)])
        dl = deltas.get(w, deltas.get("*"))
        if "D@" + w in deltas:
            say("DELTA wall=%s D=%d delta=%d (A/A rows of %s)" % (w, deltas["D@" + w], dl,
                                                                  args.aa_arm))
        elif dl is not None:
            say("DELTA wall=%s delta=%d (given)" % (w, dl))
        else:
            say("DELTA wall=%s not given: the inconclusive rule is not applied" % w)
        if not problems:
            say("COMPARE wall=%s: no problem complete in every arm" % w)
            continue
        nl = None
        if nl_arms:
            first_nl = [pr for pr in pairs if "NL" in pr]
            other = None
            if first_nl:
                a0, b0 = first_nl[0]
                other = b0 if a0 == "NL" else a0
                other = None if other == "NL" else other
            nl, tied, totals = choose_nl(nl_arms, cells, problems, other, w,
                                         args.resamples, args.seed)
            say("NL wall=%s = %s (totals %s%s)" % (
                w, nl, " ".join("%s:%d" % (k, totals[k]) for k in nl_arms),
                "; tie among %s broken conservatively against %s" % (",".join(tied), other)
                if len(tied) > 1 else ""))
        for a, b in pairs:
            ra = nl if a == "NL" else a
            rb = nl if b == "NL" else b
            two_w = None
            if cells2 is not None:
                two_w = (cells2.get(ra, {}), cells2.get(rb, {}))
            res = compare_cells(cells[ra], cells[rb], problems, w, two_w,
                                resamples=args.resamples, seed=args.seed)
            label = "%s-%s" % (a, b) + ("" if (ra, rb) == (a, b) else " (%s-%s)" % (ra, rb))
            say("TOTALS wall=%s n=%d %s: solutions=%d szs=%d | %s: solutions=%d szs=%d" % (
                w, res["n"], ra, res["total_a"], res["szs_a"], rb, res["total_b"], res["szs_b"]))
            say("COMPARE wall=%s %s n=%d gains=%d losses=%d net=%+d sign_p_one=%s sign_p_two=%s "
                "%s gate_p=%s" % (w, label, res["n"], res["gains"], res["losses"], res["net"],
                                  _fmt_p(res["p_one"]), _fmt_p(res["p_two"]), _ci_text(res["ci"]),
                                  _fmt_p(res["p_one"])))
            doms = pool_domains(res["domains"]) if sealed else res["domains"]
            best, without = drop_best(doms, res["net"])
            say("DOMAINS wall=%s %s %s%s" % (w, label, _dom_text(doms),
                                            " (domains under %d problems pooled into OTHER)"
                                            % DOMAIN_POOL_MIN if sealed else ""))
            say("DROP-BEST wall=%s %s best_domain=%s net_without_it=%+d" % (
                w, label, best, without))
            for d, v in res["domains"].items():
                dom_rows.append([w, label, d, v["n"], v["gains"], v["losses"], v["net"]])
            for side, arm in (("a", ra), ("b", rb)):
                if ("band2w_" + side) in res:
                    b2 = " misses_solved_at_%s=%d/%d" % (w2, res["band2w_" + side],
                                                          res["band2w_n_" + side])
                else:
                    b2 = " misses_solved_at_%s=no-2W-run" % w2
                say("BAND wall=%s arm=%s solves_in_(%s,%s]=%d%s" % (
                    w, arm, fmt_num(float(w) / 2), w, res["band_" + side], b2))
                lanes = res["unknown_lanes_" + side]
                by_lane = ",".join("%s:%d" % (k, lanes[k]) for k in sorted(lanes)) or "-"
                if sealed:
                    say("UNKNOWN wall=%s arm=%s unknown=%d tie_decided=%d" % (
                        w, arm, res["unknown_" + side], res["unknown_tie_" + side]))
                    lane_rows.append([w, label, arm, by_lane])
                else:
                    say("UNKNOWN wall=%s arm=%s unknown=%d tie_decided=%d by_lane=%s" % (
                        w, arm, res["unknown_" + side], res["unknown_tie_" + side], by_lane))
            if dl is not None:
                du = abs(res["unknown_a"] - res["unknown_b"])
                say("INCONCLUSIVE wall=%s %s %s (|%d-%d|=%d vs delta %d)" % (
                    w, label, "YES" if du > dl else "no", res["unknown_a"], res["unknown_b"],
                    du, dl))
            ends = []
            for side, arm in (("a", ra), ("b", rb)):
                for cls, n in sorted(res["ends_" + side].items()):
                    ends.append([w, label, arm, cls, n])
            subs_rows = []
            if all(cells[ra][p].split == "j13" for p in problems) and casc30 is not None \
                    and casc28 is not None:
                subs = j13_subsets(problems, casc30, casc28, axiom_novel, residue)
                for name, members in subs.items():
                    mem = sorted(members)
                    if not mem:
                        subs_rows.append([w, label, name, 0, 0, 0, 0, 0, 0, "-", "-"])
                        continue
                    rs = compare_cells(cells[ra], cells[rb], mem, w,
                                       resamples=args.resamples, seed=args.seed)
                    lo, hi = rs["ci"][0.95]
                    exp = J13_SUBSET_SIZES.get(name)
                    subs_rows.append([w, label, name, rs["n"], rs["total_a"], rs["total_b"],
                                      rs["gains"], rs["losses"], "%+d" % rs["net"],
                                      "[%+d,%+d]" % (lo, hi),
                                      ("size-ok" if exp in (None, rs["n"])
                                       else "size-%d-expected-%d" % (rs["n"], exp))])
            if sealed:
                sealed_sections.append(("disagreements wall=%s %s" % (w, label),
                                        ["problem", "domain", "stem", "winner", "a_time", "a_lane",
                                         "a_category", "b_time", "b_lane", "b_category",
                                         "loser_end", "loser_lane_classes"], res["table"]))
                sealed_sections.append(("miss end states wall=%s %s" % (w, label),
                                        ["wall", "pair", "arm", "end_class", "n"], ends))
                if subs_rows:
                    sealed_sections.append(("j13 subsets wall=%s %s" % (w, label),
                                            ["wall", "pair", "subset", "n", "total_a", "total_b",
                                             "gains", "losses", "net", "ci95", "check"], subs_rows))
            else:
                for e in ends:
                    say("END wall=%s %s arm=%s %s=%d" % (e[0], e[1], e[2], e[3], e[4]))
                for s in subs_rows:
                    say("SUBSET wall=%s %s %s n=%s totals=%s/%s gains=%s losses=%s net=%s ci95=%s %s"
                        % tuple(s))
                say("DISAGREEMENTS wall=%s %s n=%d" % (w, label, len(res["table"])))
                for row in res["table"]:
                    say("  " + "\t".join(str(x) for x in row))
    if sealed:
        sealed_sections.append(("unpooled domains", ["wall", "pair", "domain", "n", "gains",
                                                     "losses", "net"], dom_rows))
        sealed_sections.append(("unknown by lane", ["wall", "pair", "arm", "by_lane"], lane_rows))
        sealed_sections.append(("cells", ["wall", "arm", "problem", "split", "solution", "szs",
                                          "time", "lane", "category", "end_class",
                                          "lane_classes", "passes"], cell_rows))
        path, sha = write_sealed(args.sealed_dir, args.label, sealed_sections)
        say("SEALED per-problem tables -> %s sha256=%s (not printed; open after L2)" % (path, sha))
    elif args.table_out:
        if has_j13:
            raise SystemExit("stats: --table-out writes per-problem rows; not with J13 rows")
        with open(args.table_out, "w") as fp:
            fp.write("#wall\tarm\tproblem\tsplit\tsolution\tszs\ttime\tlane\tcategory\t"
                     "end_class\tlane_classes\tpasses\n")
            for row in cell_rows:
                fp.write("\t".join(str(x) for x in row) + "\n")
        say("TABLE per-problem cells -> %s" % args.table_out)
    print("\n".join(out))
    return rc


# ----------------------------------------------------------------------------
# delta (G0.2)


def cmd_delta(args):
    runs, _dec = load_runs(args.rows)
    has_j13 = any(r.split == "j13" for r in runs)
    look = check_look(args, has_j13, "delta")
    if has_j13 and look != "L0":
        raise SystemExit("stats: delta reads the L0 A/A rows: --look L0")
    expect = args.expect if args.expect is not None else (J13_PROBLEMS if has_j13 else None)
    walls = [fmt_num(w) for w in args.wall] if args.wall else sorted(
        {r.wall for r in runs if r.arm == args.arm}, key=float)
    print("CONDITIONS rows=%d arm=%s walls=%s look=%s expect=%s" % (
        len(runs), args.arm, ",".join(walls), look, expect if expect is not None else "all"))
    status = "PASS"
    for w in walls:
        sel = [r for r in runs if r.arm == args.arm and r.wall == w]
        t1 = sum(r.solution for r in sel if r.pass_no == 1)
        t2 = sum(r.solution for r in sel if r.pass_no == 2)
        s1 = {r.problem: r for r in sel if r.pass_no == 1}
        s2 = {r.problem: r for r in sel if r.pass_no == 2}
        common = set(s1) & set(s2)
        union = set(s1) | set(s2)
        d = sum(1 for p in common if s1[p].solution != s2[p].solution)
        dz = sum(1 for p in common if (s1[p].status == "Unsatisfiable" and s1[p].in_wall)
                 != (s2[p].status == "Unsatisfiable" and s2[p].in_wall))
        u1 = sum(1 for p in common if pass_category(s1[p]) == "unknown")
        u2 = sum(1 for p in common if pass_category(s2[p]) == "unknown")
        complete = common == union and (expect is None or len(common) == expect)
        ok = d <= G02_MAX_D
        verdict = ("PASS" if ok else "FAIL") if complete else "INCOMPLETE"
        if verdict == "INCOMPLETE":
            status = "INCOMPLETE"
        elif verdict == "FAIL" and status == "PASS":
            status = "FAIL"
        print("AA wall=%s arm=%s problems=%d pass1_solutions=%d pass2_solutions=%d D=%d "
              "D_szs=%d unknown=%d/%d delta=%s G0.2=%s (D <= %d)" % (
                  w, args.arm, len(common), t1, t2, d, dz, u1, u2,
                  delta_from_d(d) if complete else "-", verdict, G02_MAX_D))
        if not complete:
            print("AA wall=%s INCOMPLETE: %d problems with both passes, %d with either, %s "
                  "expected; neither G0.2 nor delta is decided" % (
                      w, len(common), len(union), expect if expect is not None else "all"))
    if status == "PASS":
        print("G0.2 PASS at every wall")
        return 0
    if status == "FAIL":
        print("G0.2 FAIL: find the noise source first")
        return 1
    print("G0.2 INCOMPLETE: resume the A/A runs until both passes cover every problem")
    return EXIT_INVALID


# ----------------------------------------------------------------------------
# wdep (G0.3)


def read_official(path, system):
    """problem -> official time (float) when the cell is '+TIME', else None."""
    with open(os.path.expanduser(path)) as fp:
        lines = [l.rstrip("\n") for l in fp if l.strip()]
    hdr = lines[0].split("\t")
    if system not in hdr:
        raise SystemExit("stats: system %s not in %s" % (system, path))
    i = hdr.index(system)
    out = {}
    for l in lines[1:]:
        f = l.split("\t")
        v = f[i].strip()
        out[f[0]] = float(v[1:]) if v.startswith("+") else None
    return out


def pod_solved(r):
    return r.status == "Unsatisfiable" and r.proof == 1 and r.in_wall == 1


def geomean_ratio(pairs):
    if not pairs:
        return float("nan")
    return math.exp(sum(math.log(o / p) for o, p in pairs) / len(pairs))


def wdep_analysis(runs, official, arm, min_official_s=1.0, wall_for_c=None,
                  resamples=2000, seed=DEFAULT_SEED):
    """Per wall: agreement with the official solved set, totals, the 2x2
    table; the G0.3 choice; the fallback c and 180 / c."""
    off_solved = {p for p, t in official.items() if t is not None}
    n_off = len(off_solved)
    walls = sorted({r.wall for r in runs if r.arm == arm}, key=float)
    per = collections.OrderedDict()
    for w in walls:
        sel = {r.problem: r for r in runs if r.arm == arm and r.wall == w and r.pass_no == 1}
        covered = [p for p in official if p in sel]
        solved = {p for p in covered if pod_solved(sel[p])}
        szs = {p for p in covered if sel[p].status == "Unsatisfiable" and sel[p].in_wall}
        empty = {p for p in solved if sel[p].proof_recs == 0}
        both = len(solved & off_solved)
        pod_only = len(solved - off_solved)
        off_only = len([p for p in covered if p in off_solved and p not in solved])
        neither = len(covered) - both - pod_only - off_only
        agree = (both + neither) / float(len(covered)) if covered else 0.0
        pairs = [(official[p], sel[p].wall_s) for p in sorted(solved & off_solved)]
        pairs_floor = [(o, t) for o, t in pairs if o >= min_official_s]
        per[w] = {"covered": len(covered), "total": len(solved), "szs": len(szs),
                  "empty_blocks": len(empty), "both": both,
                  "pod_only": pod_only, "official_only": off_only, "neither": neither,
                  "agreement": agree, "c_all": geomean_ratio(pairs),
                  "c_floor": geomean_ratio(pairs_floor), "n_floor": len(pairs_floor),
                  "pairs_floor": pairs_floor,
                  "rows": [(p, int(p in off_solved), int(p in solved),
                            official[p] if official[p] is not None else "-",
                            "%.2f" % sel[p].wall_s, sel[p].status, sel[p].proof, sel[p].lane or "-")
                           for p in covered]}
        q = per[w]
        q["complete"] = q["covered"] == len(official)
        q["qualifies"] = (q["complete"] and agree >= G03_MIN_AGREEMENT
                          and abs(q["total"] - n_off) <= G03_MAX_TOTAL_DIFF)
    qual = [w for w in per if per[w]["qualifies"]]
    choice = None
    if qual:
        choice = sorted(qual, key=lambda w: (-per[w]["agreement"],
                                             abs(per[w]["total"] - n_off), float(w)))[0]
    cw = wall_for_c or (max(per, key=float) if per else None)
    fallback = None
    if cw is not None and cw in per:
        pf = per[cw]["pairs_floor"]
        c = per[cw]["c_floor"]
        ci = (float("nan"), float("nan"))
        if pf:
            rng = random.Random(seed)
            gs = []
            for _ in range(resamples):
                smp = [pf[_below(rng, len(pf))] for _j in range(len(pf))]
                gs.append(geomean_ratio(smp))
            gs.sort()
            ci = percentile_ci(gs, 0.95)
        fallback = {"wall": cw, "c": c, "n": per[cw]["n_floor"], "c_ci": ci,
                    "wdep": 180.0 / c if c == c and c > 0 else float("nan")}
    return {"n_official": n_off, "per_wall": per, "choice": choice, "fallback": fallback}


def cmd_wdep(args):
    runs, _dec = load_runs(args.rows)
    has_j13 = any(r.split == "j13" for r in runs)
    look = check_look(args, has_j13, "wdep")
    if has_j13 and look != "L0":
        raise SystemExit("stats: the WDEP calibration is part of look L0: --look L0")
    sel = [r for r in runs if r.arm == args.arm]
    rss = collections.Counter(cond_field(r.cond, "rss") or "?" for r in sel)
    capped = sum(v for k, v in rss.items() if k != "default")
    print("CONDITIONS rows=%d arm=%s look=%s hosts=%d rss=%s" % (
        len(sel), args.arm, look, len({r.host for r in sel}),
        ",".join("%s:%d" % kv for kv in sorted(rss.items()))))
    walls = sorted({r.wall for r in sel}, key=float)
    validity_lines(sel, walls, print)
    if capped and not args.allow_capped:
        print("INVALID %d rows ran under another rss cap than the schedule's default; J13 ran "
              "with the default cap, so these rows cannot calibrate WDEP (--allow-capped "
              "labels the result uncalibrated)" % capped)
        return EXIT_INVALID
    official = read_official(args.official, args.system)
    res = wdep_analysis(runs, official, args.arm, args.min_official_s, args.c_wall)
    n_off = res["n_official"]
    print("OFFICIAL %s solved=%d of %d (%s)" % (args.system, n_off, len(official), args.official))
    for w, q in res["per_wall"].items():
        print("WALL %s covered=%d/%d total=%d (diff %+d) szs=%d empty_blocks=%d agreement=%.4f "
              "both=%d official_only=%d pod_only=%d neither=%d c_all=%.3f c_floor=%.3f(n=%d) %s" % (
                  w, q["covered"], len(official), q["total"], q["total"] - n_off, q["szs"],
                  q["empty_blocks"],
                  q["agreement"], q["both"], q["official_only"], q["pod_only"], q["neither"],
                  q["c_all"], q["c_floor"], q["n_floor"],
                  "QUALIFIES" if q["qualifies"] else
                  ("incomplete" if not q["complete"] else "no")))
    fb = res["fallback"]
    rc = 1
    if capped:
        print("G0.3 UNCALIBRATED: %d rows ran under the 3500 MB cap; no wall is WDEP from "
              "these rows" % capped)
    elif res["choice"] is not None:
        q = res["per_wall"][res["choice"]]
        print("G0.3 PASS: WDEP = %s s (agreement %.4f, total %d, official %d)" % (
            res["choice"], q["agreement"], q["total"], n_off))
        rc = 0
    elif fb is not None:
        print("G0.3 FAIL: no wall qualifies; WDEP = 180 / c = %.1f s, UNCALIBRATED "
              "(c = %.3f, 95%% CI [%.3f, %.3f], geometric mean of official / pod time over "
              "%d problems solved in both with official time >= %g s, %s s run)" % (
                  fb["wdep"], fb["c"], fb["c_ci"][0], fb["c_ci"][1], fb["n"],
                  args.min_official_s, fb["wall"]))
    else:
        print("G0.3 FAIL: no rows")
    rows = [[w] + list(row) for w, q in res["per_wall"].items() for row in q["rows"]]
    if rows:
        path, sha = write_sealed(args.sealed_dir, args.label, [(
            "wdep per-problem agreement", ["wall", "problem", "official_solved", "pod_solved",
                                           "official_s", "pod_s", "status", "proof", "lane"],
            rows)])
        print("SEALED per-problem agreement -> %s sha256=%s (not printed; open after L2)"
              % (path, sha))
    return rc


# ----------------------------------------------------------------------------


def build_parser():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0],
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd")

    def look_args(p):
        p.add_argument("--look", choices=LOOKS, default="dev",
                       help="L0, L1: aggregates only, the rest sealed; L2: unsealed (needs "
                            "--l2-token); dev: no J13 rows")
        p.add_argument("--l2-token", help="the plan's section 5.4 'L2 unseal token'")
        p.add_argument("--plan", default=PLAN, help="the plan (its section 5.4 registry)")

    c = sub.add_parser("compare", help="paired comparisons (section 1.5)")
    c.add_argument("--rows", nargs="+", required=True,
                   help="rows.tsv files, run_arms output directories, or .tgz archives")
    c.add_argument("--pair", action="append", required=True, metavar="A,B",
                   help="net(A - B); NL names the best of --nl; ARM@K is pass K alone")
    c.add_argument("--wall", action="append", help="walls to analyse (default: all)")
    c.add_argument("--nl", help="the NL candidates, e.g. P0,P1,PD")
    c.add_argument("--delta", action="append", metavar="N|W=N")
    c.add_argument("--aa-rows", nargs="+", help="L0 A/A rows to compute delta from")
    c.add_argument("--aa-arm", default="P0")
    look_args(c)
    c.add_argument("--label", help="name of the sealed file (looks L0, L1)")
    c.add_argument("--sealed-dir", default=SEALED_DIR)
    c.add_argument("--expect-problems", type=int,
                   help="problems a look must cover (default 400 with J13 rows)")
    c.add_argument("--casc30", default=os.path.join(J13_DIR, "casc30_ueq_names.txt"))
    c.add_argument("--casc28", default=os.path.join(J13_DIR, "casc28_ueq_names.txt"))
    c.add_argument("--axiom-novel", help="names of the axiom-novel J13 problems")
    c.add_argument("--residue", help="name TAB class for the J13 residue")
    c.add_argument("--seed", type=int, default=DEFAULT_SEED)
    c.add_argument("--resamples", type=int, default=DEFAULT_RESAMPLES)
    c.add_argument("--amendment", help="the section 5.4 entry behind a non-default seed or count")
    c.add_argument("--table-out", help="write the per-problem cells (dev rows only)")
    d = sub.add_parser("delta", help="A/A disagreement D, delta, G0.2")
    d.add_argument("--rows", nargs="+", required=True)
    d.add_argument("--arm", default="P0")
    d.add_argument("--wall", action="append")
    d.add_argument("--expect", type=int, help="problems both passes must cover (default 400 "
                                              "with J13 rows)")
    look_args(d)
    w = sub.add_parser("wdep", help="G0.3 against the official J13 result")
    w.add_argument("--rows", nargs="+", required=True)
    w.add_argument("--official", default=os.path.join(J13_DIR, "official", "ueq_official.tsv"))
    w.add_argument("--system", default="FindProof---0.1")
    w.add_argument("--arm", default="SUB")
    w.add_argument("--min-official-s", type=float, default=1.0)
    w.add_argument("--c-wall", help="the run c comes from (default: the largest wall)")
    w.add_argument("--allow-capped", action="store_true",
                   help="read rows run under the 3500 MB cap (the result is uncalibrated)")
    w.add_argument("--label", default="wdep", help="name of the sealed per-problem table")
    w.add_argument("--sealed-dir", default=SEALED_DIR)
    look_args(w)
    return ap


def main(argv=None):
    ap = build_parser()
    args = ap.parse_args(argv)
    if args.cmd == "compare":
        return cmd_compare(args)
    if args.cmd == "delta":
        return cmd_delta(args)
    if args.cmd == "wdep":
        return cmd_wdep(args)
    ap.print_help()
    return 2


if __name__ == "__main__":
    sys.exit(main())
