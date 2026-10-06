#!/usr/bin/env python3
"""Known-answer tests for the X modes of matrix_tables.py (plan amendments
A22 and A24): the job lists of the synthetic -10 corpus (--xjobs), the
validation and tables of its lane matrix (--xmatrix), and the twin-family
rule over TRAIN and TRAIN-X (--xtwins).

Synthetic data only; nothing here runs a prover.  Pods only, like every
test of this repository:
  python3 tools/atp_enigma/test_matrix_tables.py -v
"""
import contextlib
import io
import os
import shutil
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import matrix_tables as mt  # noqa: E402

LANES = ["wm", "equals", "narrowq", "equals-twee", "horn-cd", "horn-back",
         "fof-fifo", "fof-best", "fof-flat", "fof-deep", "fof-nopre",
         "fof-or2", "fof-full", "fof-split", "fof-avatar", "fof-avatar2",
         "fof-freq", "fof-sos", "fof-wall", "fof-selL", "fof-spb", "unfold",
         "unfold-tgt", "unfold-tgt-awr", "flatten", "flatten-bare", "twee",
         "twee-deep", "twee-freq", "tgt-ax", "tgt-sel", "otter-lpo",
         "otter-lpo-cmin", "otter-ladr", "mnf-gj-amw", "encompass", "bare",
         "ifeq", "tgt", "tgt-fold", "twee-raw", "bare-raw",
         "flatten-bare-raw", "wm-gj", "lazy-gj", "bare-gj"]
# Flag strings of tools/atp_cli/main.c at 6a4b244b for one lane of each
# kind, and a minimal string per group for the rest.
REAL_FLAGS = {
    "wm": "",
    "equals-twee": "--equals-goal=2 --twee-struct-dedup --preset=none "
                   "--horn=0 --simpler-join --lazy-ground-join "
                   "--backward-subsume --backward-demod --cp-weight=8 "
                   "--w2-modulo=0 --orphan-murder=0 --cp-set-interreduce "
                   "--cp-set-ir-budget=5 --max-steps=10000000 --fof-sat=0",
    "ifeq": "--ifeq-guard=2 --flatten-goal --goal-subterm --preset=none "
            "--horn=0 --fof-sat=0",
    "horn-back": "--horn-back=2 --fof-sat=0",
    "narrowq": "--narrow-queue=2 --fof-sat=0",
    "fof-fifo": "--fof-slice=2 --fol-age-ratio=1",
    "bare": "--preset=none --horn=0 --fof-sat=0",
    "twee": "--simpler-join --flatten-goal --lazy-ground-join "
            "--backward-subsume --backward-demod --cp-weight=8 "
            "--w2-modulo=0 --orphan-murder=0 --cp-set-interreduce "
            "--cp-set-ir-budget=5 --fof-sat=0",
}
GROUP_FLAGS = {"gated-dispatcher": "--horn-cd=2 --fof-sat=0",
               "gated-existential": "--equals-goal=2 --fof-sat=0",
               "horn-release": "--fof-sat=0",
               "horn-free": "--preset=none --horn=0 --fof-sat=0",
               "fof": "--fof-slice=2"}
TRAIN = ["LCL001-1-10", "LCL002-1-10", "SYN001-1-10", "SYN002+1-10",
         "GRP001-1-10", "KLE001-1-10"]
DEV = ["ALG001-1-10", "ALG002-1-10", "PUZ131+1-10"]
# Proved (problem, lane) -> wall seconds.
PROVED = {
    ("LCL001-1-10", "horn-cd"): 0.5, ("LCL001-1-10", "twee"): 2.0,
    ("LCL001-1-10", "wm"): 5.0,
    ("LCL002-1-10", "ifeq"): 12.0,
    ("SYN001-1-10", "equals"): 0.05, ("SYN001-1-10", "twee"): 0.1,
    ("SYN001-1-10", "wm"): 0.2, ("SYN001-1-10", "bare"): 0.3,
    ("KLE001-1-10", "twee"): 25.0, ("KLE001-1-10", "wm"): 35.0,
    ("ALG001-1-10", "narrowq"): 1.0,
    ("ALG002-1-10", "twee"): 3.0, ("ALG002-1-10", "bare"): 40.0,
}
HOSTS = {1: "hostA", 2: "hostA", 3: "hostB"}


def flags_text():
    groups = {l: g for g in mt.X_GROUPS for l in mt.X_LANE_GROUPS[g]}
    return "".join(f"{l}\t{REAL_FLAGS.get(l, GROUP_FLAGS[groups[l]])}\n"
                   for l in LANES)


def outcome(problem, lane):
    """The 14 runner columns of a synthetic job."""
    split = "TRAIN-X" if problem in TRAIN else "DEV-X"
    if (problem, lane) in PROVED:
        w = PROVED[(problem, lane)]
        return [problem, lane, split, "0", "Unsatisfiable", f"{w:.2f}",
                f"{w:.2f}", "20000", "proved", "100", "10", "1000", "exit 0",
                "PROVED"]
    if lane.startswith("fof-") or (
            lane in ("equals", "narrowq", "equals-twee", "horn-cd",
                     "horn-back", "ifeq")):
        return [problem, lane, split, "1", "GaveUp", "0.01", "0.00", "2600",
                "fast-exit-not-applicable", "", "", "", "exit 1",
                f"{lane} slice: not applicable"]
    if problem == "SYN002+1-10":
        return [problem, lane, split, "1", "GaveUp", "20.00", "20.00",
                "4700000", "memory-guard", "5000", "300", "900000", "exit 1",
                "ABORTED (memory guard / abort hook)"]
    return [problem, lane, split, "1", "Timeout", "59.50", "59.00",
            "900000", "wall", "50000", "800", "2000000", "",
            "stopped at the wall by the lane parent"]


def started(k, host=None, par=14, sha=mt.X_BIN_SHA, nproc=16):
    host = host or HOSTS[k]
    return (f"started=2026-10-06T10:00:00Z\n"
            f"host={host} nproc={nproc} mem_gb=62 cpu=AMD EPYC-Genoa Processor\n"
            f"par={par} wall=60 timeout=75 deadline_s=14400 limit=0 env=\n"
            f"bin=/home/ubuntu/xm/thvm/bin/FindProof sha256={sha}\n"
            f"shard=/home/ubuntu/xm/in/slots/slot_{k}.tsv jobs=138 pending=138\n"
            f"tptp_root=/home/ubuntu/xm/xroot j13_root=/home/ubuntu/xm/noj13\n")


class XModes(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix="test_matrix_tables_")
        self.saved = (mt.X_SLOTS, dict(mt.X_CAP))
        mt.X_SLOTS = 3
        t = self.tmp
        self.corpus = os.path.join(t, "corpus")
        for split, names, label in (("TRAIN-X", TRAIN, "train"),
                                    ("DEV-X", DEV, "heldout")):
            lines = []
            for n in names:
                dom = n[:3]
                rel = f"Problems/{dom}/{n}.p"
                os.makedirs(os.path.join(self.corpus, split, "Problems", dom),
                            exist_ok=True)
                with open(os.path.join(self.corpus, split, rel), "w") as fp:
                    fp.write(f"cnf(c,axiom, a = b ).  % {n}\n")
                lines.append(f"{n}\t{rel}\tUnsatisfiable\tCNF_UNS_RFO_PEQ_UEQ"
                             f"\t{label}\n")
            with open(os.path.join(self.corpus, split, "pool.tsv"), "w") as fp:
                fp.write("".join(lines))
        self.j13 = os.path.join(t, "j13_names.txt")
        with open(self.j13, "w") as fp:
            fp.write("BOO001-1\nLAT999-10\n")
        self.lanes = os.path.join(t, "lanes.txt")
        with open(self.lanes, "w") as fp:
            fp.write("".join(l + "\n" for l in LANES))
        self.xdir = os.path.join(t, "x")
        os.makedirs(self.xdir)
        self.xp = mt.XPaths(self.xdir, self.corpus, self.j13, self.lanes,
                            os.path.join(t, "sealed"))

    def tearDown(self):
        mt.X_SLOTS, mt.X_CAP = self.saved[0], self.saved[1]
        shutil.rmtree(self.tmp)

    def quiet(self, fn, *a):
        with contextlib.redirect_stdout(io.StringIO()):
            return fn(*a)

    def read(self, *p):
        with open(os.path.join(self.xdir, *p)) as fp:
            return fp.read()

    def tsv(self, *p):
        lines = self.read(*p).splitlines()
        head = lines[0].split("\t")
        return [dict(zip(head, l.split("\t"))) for l in lines[1:]]

    def results(self, skip=(), extra_rows=None, sha=mt.X_BIN_SHA):
        """Runner output for every job of the slot lists, sentinels and pod
        records, as a harvest leaves them."""
        res = os.path.join(self.xdir, "results")
        os.makedirs(res, exist_ok=True)
        for k in range(1, mt.X_SLOTS + 1):
            rows = []
            for j in mt.x_jobs(self.xp, k):
                if (j[0], j[3]) in skip:
                    continue
                rows.append("\t".join(outcome(j[0], j[3])))
            rows += (extra_rows or {}).get(k, [])
            with open(os.path.join(res, f"slot_{k}.tsv"), "w") as fp:
                fp.write("".join(r + "\n" for r in rows))
            with open(os.path.join(res, f"slot_{k}.tsv.started"), "w") as fp:
                fp.write(started(k, sha=sha))
            with open(os.path.join(res, f"slot_{k}.tsv.done"), "w") as fp:
                fp.write("status=complete\nfinished=2026-10-06T11:00:00Z "
                         "elapsed_s=3600\nshard_jobs=138 with_row=138 "
                         "pending_left=0\n")
        for pod, host in (("atp-x-1", "hostA"), ("atp-x-2", "hostB")):
            d = os.path.join(res, "pods", pod)
            os.makedirs(d, exist_ok=True)
            with open(os.path.join(d, "hostname.txt"), "w") as fp:
                fp.write(host + "\n")
            with open(os.path.join(d, "dmesg_oom.txt"), "w") as fp:
                fp.write("")
            with open(os.path.join(d, "list_lanes_flags.txt"), "w") as fp:
                fp.write(flags_text())

    def test_stem_and_groups(self):
        self.assertEqual(mt.x_stem("PUZ131+1-10"), "PUZ131")
        self.assertEqual(mt.x_stem("ALG003-1-10"), "ALG003")
        self.assertEqual(mt.stem("PUZ131+1-10"), "PUZ131+1")
        with self.assertRaises(SystemExit):
            mt.x_stem("ab-10")
        groups = {l: g for g in mt.X_GROUPS for l in mt.X_LANE_GROUPS[g]}
        self.assertEqual(sorted(groups), sorted(LANES))
        for lane, flags in REAL_FLAGS.items():
            self.assertEqual(mt.x_group(flags), groups[lane], lane)
        self.assertEqual(mt.X_LANE_GROUPS["gated-dispatcher"]
                         + mt.X_LANE_GROUPS["gated-existential"],
                         ("horn-cd", "horn-back", "ifeq", "equals", "narrowq",
                          "equals-twee"))

    def test_xjobs(self):
        self.quiet(mt.mode_xjobs, self.xp)
        probs = [l.split("\t") for l in self.read("problems.tsv").splitlines()]
        self.assertEqual([p[0] for p in probs],
                         ["GRP001-1-10", "KLE001-1-10", "LCL001-1-10",
                          "LCL002-1-10", "SYN001-1-10", "SYN002+1-10",
                          "ALG001-1-10", "ALG002-1-10", "PUZ131+1-10"])
        self.assertEqual(probs[5], ["SYN002+1-10",
                                    "TRAIN-X/Problems/SYN/SYN002+1-10.p",
                                    "TRAIN-X", "SYN002"])
        jobs = []
        for k in (1, 2, 3):
            js = mt.x_jobs(self.xp, k)
            self.assertEqual(len(js), 3 * len(LANES))
            self.assertEqual(len({j[0] for j in js}), 3)
            jobs += js
        self.assertEqual(self.read("jobs.tsv"),
                         "".join("\t".join(j) + "\n" for j in jobs))
        firsts = {k: {j[0] for j in mt.x_jobs(self.xp, k)} for k in (1, 2, 3)}
        self.assertEqual(firsts[1], {"GRP001-1-10", "LCL002-1-10",
                                     "ALG001-1-10"})
        sums = self.read("corpus_files.sha256").splitlines()
        self.assertEqual(len(sums), len(TRAIN) + len(DEV) + 2)
        self.assertIn("TRAIN-X/pool.tsv", [s.split("  ")[1] for s in sums])
        self.assertIn("jobs.tsv sha256", self.read("jobs.txt"))

    def test_xjobs_refuses_a_j13_stem_and_the_cap(self):
        with open(self.j13, "a") as fp:
            fp.write("SYN001-10\n")
        with self.assertRaises(SystemExit):
            self.quiet(mt.mode_xjobs, self.xp)
        with open(self.j13, "w") as fp:
            fp.write("BOO001-1\n")
        mt.X_CAP["DEV-X"] = 2
        with self.assertRaises(SystemExit):
            self.quiet(mt.mode_xjobs, self.xp)

    def test_xmatrix_tables(self):
        self.quiet(mt.mode_xjobs, self.xp)
        self.results()
        self.assertTrue(self.quiet(mt.mode_xmatrix, self.xp))
        v = self.read("validate.txt")
        self.assertIn("overall: PASS", v)
        self.assertEqual(len(self.read("xmatrix.tsv").splitlines()),
                         9 * len(LANES))
        t1 = {r["lane"]: r for r in self.tsv("tables", "T1_lanes_TRAIN-X.tsv")}
        self.assertEqual((t1["twee"]["proved"], t1["wm"]["proved"],
                          t1["ifeq"]["proved"], t1["ifeq"]["proved_alone"]),
                         ("3", "3", "1", "1"))
        curve = {r["t_s"]: r["vbs_solved"]
                 for r in self.tsv("tables", "T2_vbs_curve_TRAIN-X.tsv")}
        self.assertEqual((curve["10"], curve["30"], curve["59.50"]),
                         ("2", "4", "4"))
        cover = self.tsv("tables", "T2_greedy_cover_TRAIN-X.tsv")
        self.assertEqual([(r["lane"], r["gain"]) for r in cover],
                         [("wm", "3"), ("ifeq", "1")])
        tail = {r["problem"]: r for r in self.tsv("tables",
                                                  "T4_tail_TRAIN-X.tsv")}
        self.assertEqual(sorted(tail), ["KLE001-1-10", "LCL002-1-10",
                                        "SYN002+1-10"])
        self.assertEqual((tail["LCL002-1-10"]["owner"],
                          tail["KLE001-1-10"]["owner"],
                          tail["SYN002+1-10"]["condition"],
                          tail["SYN002+1-10"]["stem"]),
                         ("ifeq", "twee", "all-searchers-memory-guard",
                          "SYN002"))
        g = {r["group"]: r for r in self.tsv("tables", "X10_groups_TRAIN-X.tsv")}
        self.assertEqual((g["gated (both)"]["solved_union"],
                          g["gated (both)"]["solved_only_by_group"]),
                         ("3", "1"))
        self.assertEqual([g["all but gated"][c] for c in ("vbs_le10s",
                                                          "vbs_le30s",
                                                          "vbs_at60s")],
                         ["2", "3", "3"])
        self.assertEqual([g["all"][c] for c in ("vbs_le10s", "vbs_le30s",
                                                "vbs_at60s")],
                         ["2", "4", "4"])
        only = self.tsv("tables", "X10_only_gated_TRAIN-X.tsv")
        self.assertEqual([r["problem"] for r in only], ["LCL002-1-10"])
        x10 = {r["lane"]: r for r in self.tsv("tables", "X10_lanes_TRAIN-X.tsv")}
        self.assertEqual((x10["horn-cd"]["group"], x10["horn-cd"]["proved"],
                          x10["horn-cd"]["fastest_strict"],
                          x10["horn-cd"]["fast_exit"],
                          x10["ifeq"]["cover_pick"], x10["wm"]["cover_pick"],
                          x10["twee"]["cover_pick"]),
                         ("gated-dispatcher", "1", "1", "5", "2", "1", "-"))
        dev = {r["t_s"]: r["vbs_solved"]
               for r in self.tsv("tables", "T2_vbs_curve_DEV-X.tsv")}
        self.assertEqual((dev["10"], dev["59.50"]), ("2", "2"))
        lab = [l.split("\t") for l in
               self.read("label_harvest_jobs.tsv").splitlines()]
        self.assertEqual(len(lab), 10)
        self.assertEqual({(l[0], l[3]) for l in lab},
                         {k for k in PROVED if k[0] in TRAIN})
        self.assertTrue(all(l[2] == "TRAIN-X" and l[1].startswith("TRAIN-X/")
                            for l in lab))
        s = self.read("tables", "summary.txt")
        self.assertIn("VBS at 10 s 2, at 30 s 4, at 60 s 4 of 6", s)
        self.assertIn("best single lane at 60 s: wm, twee 3", s)
        self.assertIn("hard tail, fastest lane over 10 s: 2", s)
        self.assertIn("label harvest: every TRAIN-X (problem, lane) pair "
                      "proved at 60 s: 10 pairs on 4 problems", s)

    def test_xmatrix_missing_row(self):
        self.quiet(mt.mode_xjobs, self.xp)
        self.results(skip={("GRP001-1-10", "wm")})
        with self.assertRaises(SystemExit):
            self.quiet(mt.mode_xmatrix, self.xp)
        v = self.read("validate.txt")
        self.assertIn("overall: FAIL", v)
        self.assertIn("missing 1", v)

    def test_xmatrix_sentinels_and_claims(self):
        self.quiet(mt.mode_xjobs, self.xp)
        self.results(sha="0" * 64)
        self.assertFalse(self.quiet(mt.mode_xmatrix, self.xp))
        self.assertIn("[FAIL] sentinels", self.read("validate.txt"))
        self.results(skip={("GRP001-1-10", "bare")}, extra_rows={1: [
            "GRP001-1-10\tbare\tTRAIN-X\t0\tSatisfiable\t1.00\t1.00\t9000\t"
            "proved\t10\t1\t10\texit 0\tPROVED"]})
        self.assertFalse(self.quiet(mt.mode_xmatrix, self.xp))
        v = self.read("validate.txt")
        self.assertIn("[FAIL] row sanity", v)
        self.assertIn("satisfiable claim: GRP001-1-10 bare", v)

    def test_xmatrix_oom_rerun(self):
        self.quiet(mt.mode_xjobs, self.xp)
        killed = ("GRP001-1-10\tbare\tTRAIN-X\t1\t\t30.00\t29.00\t4800000\t"
                  "crash\t\t\t\tsignal 9\tslice signal 9")
        self.results(skip={("GRP001-1-10", "bare")},
                     extra_rows={1: [killed]})
        self.assertFalse(self.quiet(mt.mode_xmatrix, self.xp))
        self.assertIn("[FAIL] kernel OOM", self.read("validate.txt"))
        res = os.path.join(self.xdir, "results")
        with open(os.path.join(res, "rerun_1.tsv"), "w") as fp:
            fp.write("\t".join(outcome("GRP001-1-10", "bare")) + "\n")
        with open(os.path.join(res, "rerun_1.tsv.started"), "w") as fp:
            fp.write(started(1, par=7))
        self.assertTrue(self.quiet(mt.mode_xmatrix, self.xp))
        rows = [l.split("\t") for l in self.read("xmatrix.tsv").splitlines()]
        r = [x for x in rows if x[0] == "GRP001-1-10" and x[1] == "bare"]
        self.assertEqual((len(r), r[0][8], r[0][15]), (1, "wall", "7"))
        with open(os.path.join(res, "pods", "atp-x-2", "dmesg_oom.txt"),
                  "w") as fp:
            fp.write("[Mon Oct  6 10:00:00 2026] Out of memory: Killed process "
                     "123 (FindProof)\n")
        self.assertFalse(self.quiet(mt.mode_xmatrix, self.xp))

    def test_xmatrix_lane_groups_against_flags(self):
        self.quiet(mt.mode_xjobs, self.xp)
        self.results()
        p = os.path.join(self.xdir, "results", "pods", "atp-x-2",
                         "list_lanes_flags.txt")
        with open(p, "w") as fp:
            fp.write(flags_text().replace("bare\t--preset=none --horn=0",
                                          "bare\t--preset=none"))
        self.assertFalse(self.quiet(mt.mode_xmatrix, self.xp))
        self.assertIn("[FAIL] lane groups", self.read("validate.txt"))

    def test_t4_default_stem(self):
        rows = []
        for lane in ("wm", "twee"):
            r = mt.Row()
            r.problem, r.lane, r.split = "GRP001-1", lane, "train"
            r.exit, r.szs, r.cpu, r.rss = 0, "Unsatisfiable", 11.0, 100
            r.wall = 11.0 if lane == "wm" else 12.0
            r.cls, r.steps, r.rules, r.cps = "proved", 1, 1, 1
            r.slice_end, r.detail, r.shard, r.par = "exit 0", "PROVED", 1, 16
            rows.append(r)
        pool = mt.Pool(rows, {"GRP001-1": ("Problems/GRP/GRP001-1.p",
                                           "train")}, ["wm", "twee"])
        out = os.path.join(self.tmp, "t4")
        os.makedirs(out)

        def T(name):
            return os.path.join(out, name + ".tsv")

        mt.tables_t1_t4(pool, ["GRP001-1"], ["wm", "twee"], T, mt.WITHIN,
                        mt.VBS_GRID, mt.hard_tail(pool, ["GRP001-1"]))
        with open(T("T4_tail")) as fp:
            last = fp.read().splitlines()[1].split("\t")
        self.assertEqual((last[0], last[3], last[-1]),
                         ("GRP001-1", "wm", "GRP001"))


# --xtwins (amendment A24).  Pool: two TRAIN problems, a J13 stem-mate and
# a DEV-T train problem (both out of TRAIN), a heldout one.  TRAIN-T owners
# come from train_t.tsv.  X: five TRAIN-X problems whose fastest lane needs
# more than 10 s, one where every lane ends on the memory guard, one easy
# one, and a DEV-X problem that must not count.
POOL = {"LAT001-1": "train", "GRP001-1": "train", "BOO001-2": "train",
        "REL001-1": "train", "COL001-1": "heldout"}
TRAIN_T = {"LAT001-1": "encompass", "GRP001-1": "horn-cd,twee"}
XSPLIT = {"LCL001-1-10": "TRAIN-X", "LCL002-1-10": "TRAIN-X",
          "SYN001-1-10": "TRAIN-X", "KLE001-1-10": "TRAIN-X",
          "GRP002-1-10": "TRAIN-X", "ALG001-1-10": "TRAIN-X",
          "PUZ001-1-10": "TRAIN-X", "LAT001-1-10": "DEV-X"}
XPROVED = {("LCL001-1-10", "tgt-sel"): 12.0, ("LCL001-1-10", "twee"): 20.0,
           ("LCL002-1-10", "tgt-sel"): 15.0,
           ("LCL002-1-10", "twee-deep"): 15.0,
           ("SYN001-1-10", "horn-cd"): 11.0, ("KLE001-1-10", "wm"): 30.0,
           ("GRP002-1-10", "tgt-ax"): 40.0, ("ALG001-1-10", "twee"): 2.0,
           ("LAT001-1-10", "bare"): 20.0}


def rate(problem, lane, side):
    """Selections per CPU second: 1000 (1000 us per selection) except
    tgt-ax, 10000 everywhere (100 us, under every bar), and wm, 10000 on
    the pool (it passes the bar only pooled).  The problems outside TRAIN
    select at 1 per second, so including them would move the medians."""
    if side == "pool" and problem in ("BOO001-2", "REL001-1"):
        return 1
    if lane == "tgt-ax" or (lane == "wm" and side == "pool"):
        return 10000
    return 1000


def row16(problem, lane, split, side):
    if (problem, lane) in XPROVED:
        w, cls, ex, szs = XPROVED[(problem, lane)], "proved", "0", \
            "Unsatisfiable"
    elif problem == "PUZ001-1-10":
        w, cls, ex, szs = 20.0, "memory-guard", "1", "GaveUp"
    else:
        w, cls, ex, szs = 59.5, "wall", "1", "Timeout"
    steps = rate(problem, lane, side) * int(w)
    return "\t".join([problem, lane, split, ex, szs, f"{w:.2f}",
                      f"{int(w)}.00", "20000", cls, str(steps), "10", "1000",
                      "", "synthetic", "1", "14"])


class XTwins(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix="test_matrix_tables_xtwins_")
        t = self.tmp
        self.m = os.path.join(t, "matrix")
        self.x = os.path.join(t, "x")
        os.makedirs(self.m)
        os.makedirs(self.x)

        def w(path, text):
            with open(path, "w") as fp:
                fp.write(text)

        lanes = "".join(l + "\n" for l in LANES)
        w(os.path.join(self.m, "lanes.txt"), lanes)
        w(os.path.join(self.x, "lanes.txt"), lanes)
        w(os.path.join(self.m, "problems.tsv"),
          "".join(f"{p}\tProblems/{p[:3]}/{p}.p\t{s}\ttptp\n"
                  for p, s in POOL.items())
          + "BOO001-1\tUEQ/BOO001-1.p\tj13\tj13local\n")
        w(os.path.join(self.m, "matrix.tsv"),
          "".join(row16(p, l, s, "pool") + "\n"
                  for p, s in POOL.items() for l in LANES))
        w(os.path.join(self.m, "train_t.tsv"),
          "problem\tcondition\tfastest_s\towner\tstem\tstem_hash\n"
          + "".join(f"{p}\tfastest>10s\t11.00\t{o}\t{p[:6]}\t60\n"
                    for p, o in TRAIN_T.items()))
        w(os.path.join(self.m, "dev_t.tsv"),
          "problem\tsplit\tcondition\tfastest_s\towner\tstem\tstem_hash\n"
          "REL001-1\ttrain\tfastest>10s\t12.00\twm\tREL001\t10\n"
          "COL001-1\theldout\tfastest>10s\t13.00\twm\tCOL001\t20\n")
        w(os.path.join(self.x, "problems.tsv"),
          "".join(f"{p}\t{s}/Problems/{p[:3]}/{p}.p\t{s}\t{p[:6]}\n"
                  for p, s in XSPLIT.items()))
        w(os.path.join(self.x, "xmatrix.tsv"),
          "".join(row16(p, l, s, "x") + "\n"
                  for p, s in XSPLIT.items() for l in LANES))
        self.j13 = os.path.join(t, "j13_names.txt")
        w(self.j13, "BOO001-1\n")
        sealed = os.path.join(t, "sealed")
        self.paths = mt.Paths(self.m, sealed)
        self.xp = mt.XPaths(self.x, os.path.join(t, "corpus"), self.j13,
                            os.path.join(self.x, "lanes.txt"), sealed)

    def tearDown(self):
        shutil.rmtree(self.tmp)

    def run_mode(self):
        with contextlib.redirect_stdout(io.StringIO()):
            mt.mode_xtwins(self.paths, self.xp)
        with open(os.path.join(self.x, "decisions_a24.txt")) as fp:
            return fp.read()

    def test_known_answer(self):
        t = self.run_mode()
        # Owners: tgt-sel 2 (one tie with twee-deep), horn-cd 2 (TRAIN-T's
        # tie with twee, and SYN001), then 1 each, in SCHEDULE order: wm,
        # twee, twee-deep, tgt-ax, encompass.  horn-cd is ineligible (A5),
        # tgt-ax is under 700 us per selection, wm passes only pooled.
        self.assertIn("at least 700 us per selection): tgt-sel, wm, twee, "
                      "twee-deep\n", t)
        self.assertIn("(a family that fails G2.1 or G2.2): encompass\n", t)
        self.assertIn("  pooled, head 113 us: none\n", t)
        self.assertIn("head 35 us: twee, encompass\n", t)
        self.assertIn("  TRAIN-X alone, head 35 us: tgt-sel, wm, "
                      "twee-deep\n", t)
        self.assertIn("the TRAIN-X hard tail, 6 of 7 problems (section 1.4: "
                      "all-searchers-memory-guard 1, fastest>10s 5;", t)
        self.assertIn("pooled over TRAIN (2 problems", t)
        wm = [l for l in t.splitlines() if l.startswith("   wm ")][0].split()
        self.assertEqual(wm[1:], ["0", "1", "1", "100", "1000", "1000"])
        self.assertIn("  LCL002-1-10\tfastest>10s\t15.00\ttwee-deep,tgt-sel",
                      t)
        self.assertNotIn("LAT001-1-10", t)

    def test_refuses_a_j13_stem_mate_in_train_t(self):
        with open(os.path.join(self.m, "train_t.tsv"), "a") as fp:
            fp.write("BOO001-2\tfastest>10s\t11.00\twm\tBOO001\t60\n")
        with self.assertRaises(SystemExit):
            with contextlib.redirect_stderr(io.StringIO()):
                self.run_mode()


if __name__ == "__main__":
    unittest.main()
