#!/usr/bin/env python3
"""Known-answer tests for stats.py and the pure parsers of run_arms.py.

Synthetic data only; nothing here runs a prover or a checker.
  python3 tools/atp_enigma/protocol/test_stats.py -v
"""
import contextlib
import gzip
import io
import math
import os
import shutil
import stat
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import run_arms  # noqa: E402
import stats  # noqa: E402

REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))


def row(problem, arm, wall, pass_no, sol, split="heldout", wall_s=1.0, lane="twee",
        status=None, proof=None, verdict=None, in_wall=1, end_class=None, host="podA",
        lane_classes=""):
    """One rows.tsv row in run_arms.py's format."""
    if status is None:
        status = "Unsatisfiable" if sol else "Timeout"
    if proof is None:
        proof = 1 if status == "Unsatisfiable" else 0
    if verdict is None:
        verdict = "VerifiedGood" if sol else ("Unknown" if status == "Unsatisfiable" else "-")
    if end_class is None:
        end_class = "proved" if status == "Unsatisfiable" else "wall"
    vals = {
        "problem": problem, "split": split, "arm": arm, "wall": str(wall), "pass": str(pass_no),
        "status": status, "wall_s": "%.2f" % wall_s, "lane": lane if status == "Unsatisfiable" else "",
        "proof": str(proof), "verdict": verdict, "solution": str(int(sol)),
        "lscpu": "AMD EPYC 9554|tpc=2|cpus=16|cps=8|sockets=1", "bin_sha": "ab" * 32,
        "in_wall": str(in_wall), "rc": "0" if sol else "1", "cpu_s": "1.0", "maxrss_kb": "1000",
        "end_class": end_class, "lane_classes": lane_classes,
        "proof_recs": "12" if proof else "0", "check_s": "0.5", "strays": "0",
        "host": host, "cond": "maxpar=16;rss=default;errdir=1;proof=1;check_wall=60",
        "utc": "2026-10-01T00:00:00Z",
    }
    return [vals[c] for c in run_arms.ROW_COLUMNS]


def write_rows(path, rows, decisions=None):
    os.makedirs(path, exist_ok=True)
    with open(os.path.join(path, "rows.tsv"), "w") as fp:
        fp.write("#" + "\t".join(run_arms.ROW_COLUMNS) + "\n")
        for r in rows:
            fp.write("\t".join(r) + "\n")
    if decisions is not None:
        with open(os.path.join(path, "decisions.tsv"), "w") as fp:
            fp.write("#" + "\t".join(run_arms.DECISION_COLUMNS) + "\n")
            for d in decisions:
                fp.write("\t".join(str(x) for x in d) + "\n")


def run_cli(argv):
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        rc = stats.main(argv)
    return rc, buf.getvalue()


class SignTest(unittest.TestCase):
    def test_plan_examples(self):
        # Section 1.5: 5 gains and 0 losses pass, as do 8 gains and 1 loss.
        self.assertAlmostEqual(stats.sign_test(5, 0)[0], 1 / 32.0)
        self.assertAlmostEqual(stats.sign_test(8, 1)[0], 10 / 512.0)
        self.assertLess(stats.sign_test(5, 0)[0], 0.05)
        self.assertLess(stats.sign_test(8, 1)[0], 0.05)
        self.assertGreaterEqual(stats.sign_test(4, 0)[0], 0.05)
        self.assertAlmostEqual(stats.sign_test(4, 0)[0], 1 / 16.0)

    def test_two_sided(self):
        self.assertAlmostEqual(stats.sign_test(5, 0)[1], 2 / 32.0)
        p1, p2 = stats.sign_test(3, 3)
        self.assertAlmostEqual(p1, 42 / 64.0)
        self.assertEqual(p2, 1.0)
        self.assertEqual(stats.sign_test(0, 0), (1.0, 1.0))
        # 0 gains, 6 losses: one-sided P(X >= 0) = 1; two-sided 2/64.
        p1, p2 = stats.sign_test(0, 6)
        self.assertEqual(p1, 1.0)
        self.assertAlmostEqual(p2, 2 / 64.0)


class Delta(unittest.TestCase):
    def test_table(self):
        want = {0: 2, 1: 2, 4: 2, 5: 3, 9: 3, 10: 4, 12: 4, 16: 4, 17: 4, 100: 4}
        for d, dl in want.items():
            self.assertEqual(stats.delta_from_d(d), dl, d)


class Bootstrap(unittest.TestCase):
    def test_degenerate(self):
        diffs = {"P%03d-1" % i: 1 for i in range(20)}
        stems = {p: stats.stem_of(p) for p in diffs}
        self.assertEqual(len(set(stems.values())), 20)
        ci = stats.cluster_ci(diffs, stems, resamples=500)
        self.assertEqual(ci[0.95], (20, 20))
        zero = {p: 0 for p in diffs}
        self.assertEqual(stats.cluster_ci(zero, stems, resamples=500)[0.95], (0, 0))
        one = {"COL042-%d" % i: v for i, v in enumerate([1, 1, 1, 0, 0])}
        st = {p: stats.stem_of(p) for p in one}
        self.assertEqual(set(st.values()), {"COL042"})
        self.assertEqual(stats.cluster_ci(one, st, resamples=500)[0.975], (3, 3))

    def test_two_stems_exact(self):
        # Stems a (+2) and b (0), two draws: nets 0, 2, 4 with 1/4, 1/2, 1/4.
        diffs = {"AAA001-1": 1, "AAA001-2": 1, "BBB001-1": 0}
        stems = {p: stats.stem_of(p) for p in diffs}
        nets = stats.bootstrap_nets(diffs, stems, resamples=10000, seed=7)
        f = {v: nets.count(v) / 10000.0 for v in set(nets)}
        self.assertEqual(set(f), {0, 2, 4})
        self.assertAlmostEqual(f[0], 0.25, delta=0.02)
        self.assertAlmostEqual(f[2], 0.50, delta=0.02)
        self.assertAlmostEqual(f[4], 0.25, delta=0.02)
        self.assertEqual(stats.percentile_ci(nets, 0.95), (0, 4))

    def test_clustering_widens(self):
        # One stem of 10 problems all +1, ten stems of one problem at 0:
        # net* = 10 K, K ~ Binomial(11, 1/11); CI95 = [0, 30] exactly
        # (P(K <= 2) = 0.929 < 0.975 <= P(K <= 3) = 0.987; P(K = 0) = 0.35).
        diffs = {"GRP001-%d" % i: 1 for i in range(10)}
        diffs.update({"LAT%03d-1" % i: 0 for i in range(10)})
        stems = {p: stats.stem_of(p) for p in diffs}
        self.assertEqual(stats.cluster_ci(diffs, stems)[0.95], (0, 30))
        flat = {p: p for p in diffs}
        lo, hi = stats.cluster_ci(diffs, flat)[0.95]
        self.assertTrue(4 <= lo <= 8 and 12 <= hi <= 16, (lo, hi))

    def test_reproducible_and_levels(self):
        diffs = {"P%03d-1" % i: (1 if i % 3 == 0 else (-1 if i % 5 == 0 else 0))
                 for i in range(90)}
        stems = {p: stats.stem_of(p) for p in diffs}
        a = stats.cluster_ci(diffs, stems)
        b = stats.cluster_ci(diffs, stems)
        self.assertEqual(a, b)
        lo95, hi95 = a[0.95]
        lo975, hi975 = a[0.975]
        self.assertLessEqual(lo975, lo95)
        self.assertGreaterEqual(hi975, hi95)
        net = sum(diffs.values())
        self.assertTrue(lo95 <= net <= hi95)

    def test_percentile_indices(self):
        nets = list(range(10000))
        self.assertEqual(stats.percentile_ci(nets, 0.95), (249, 9749))
        self.assertEqual(stats.percentile_ci(nets, 0.975), (124, 9874))


class Names(unittest.TestCase):
    def test_stem_domain_minus10(self):
        self.assertEqual(stats.stem_of("COL042-6"), "COL042")
        self.assertEqual(stats.stem_of("NUN135-1.006"), "NUN135")
        self.assertEqual(stats.domain_of("NUM284-10.014"), "NUM")
        for n in ("LCL054-10", "NUM284-10.014", "ALG212-10"):
            self.assertTrue(stats.is_minus10(n), n)
        for n in ("GRP415-1", "NUN135-1.006", "GRP664-11", "LAT400-1"):
            self.assertFalse(stats.is_minus10(n), n)

    def test_subsets_synthetic(self):
        probs = ["A%02d-1" % i for i in range(10)]
        probs = ["GRP%03d-1" % i for i in range(10)] + ["LCL%03d-10" % i for i in range(4)]
        c30 = probs[:6] + ["ZZZ999-1"]
        c28 = probs[4:9]
        subs = stats.j13_subsets(probs, c30, c28, axiom_novel=probs[:2],
                                 residue={probs[13]: "ac", probs[12]: "existential"})
        self.assertEqual(len(subs["casc30-recurring"]), 6)
        self.assertEqual(subs["casc28-only"], set(probs[6:9]))
        self.assertEqual(subs["new"], set(probs[9:]))
        self.assertEqual(len(subs["minus10"]), 4)
        self.assertEqual(len(subs["other-than-minus10"]), 10)
        self.assertEqual(subs["axiom-novel"], set(probs[:2]))
        self.assertEqual(subs["residue:ac"], {probs[13]})

    def test_subsets_real_lists(self):
        # Problem names only (the J13 problem file names and the CASC name
        # lists; no outcome is read): the subset sizes of section 1.5.
        ueq = os.path.join(stats.J13_DIR, "UEQ")
        c30 = os.path.join(stats.J13_DIR, "casc30_ueq_names.txt")
        c28 = os.path.join(stats.J13_DIR, "casc28_ueq_names.txt")
        if not all(os.path.exists(f) for f in (ueq, c30, c28)):
            self.skipTest("J13 name lists not present")
        names = [f[:-2] for f in os.listdir(ueq) if f.endswith(".p")]
        self.assertEqual(len(names), 400)
        subs = stats.j13_subsets(names, stats.read_names(c30), stats.read_names(c28))
        for k, v in stats.J13_SUBSET_SIZES.items():
            self.assertEqual(len(subs[k]), v, k)


class Cells(unittest.TestCase):
    def cell(self, sols, times=None, cats=None):
        times = times or [10.0 + i for i in range(len(sols))]
        rs = []
        for i, s in enumerate(sols):
            kw = {}
            if cats and cats[i] == "unknown":
                kw = {"status": "Unsatisfiable", "verdict": "Unknown", "lane": "equals"}
            rs.append(stats.Run(dict(zip(run_arms.ROW_COLUMNS,
                                         row("GRP001-1", "E", 180, i + 1, s, wall_s=times[i],
                                             **kw)))))
        return stats.Cell(rs)

    def test_majority_of_three(self):
        c = self.cell([1, 0, 1], times=[30.0, 50.0, 20.0])
        self.assertEqual(c.sol, 1)
        self.assertEqual(c.time, 30.0)
        self.assertEqual(self.cell([1, 0, 0]).sol, 0)
        self.assertEqual(self.cell([1]).sol, 1)
        self.assertEqual(self.cell([0]).sol, 0)
        two = self.cell([1, 0])
        self.assertEqual((two.sol, two.time), (0, stats.INF))

    def test_unknown_category(self):
        c = self.cell([0, 0, 1], cats=["unknown", "unknown", None])
        self.assertEqual((c.sol, c.category, c.unknown_lane), (0, "unknown", "equals"))
        c = self.cell([0, 0, 0], cats=["unknown", None, None])
        self.assertEqual(c.category, "miss")
        c = self.cell([0, 0], cats=["unknown", None])
        self.assertEqual(c.category, "unknown")  # ties go to Unknown


def synthetic_j13(tmp):
    """Arms P0, P1, PD, E on 40 J13-split problems at W=180, with known
    outcomes; repeats on 3 problems.  Returns (dir, problem names, facts)."""
    names = (["GRP%03d-1" % i for i in range(16)] + ["LCL%03d-10" % i for i in range(12)]
             + ["LAT%03d-1" % i for i in range(12)])
    base = set(names[:30])                      # P0 and PD solve 30
    sets = {"P0": set(base), "P1": set(base) | {names[30]},      # 31
            "PD": set(base),
            "E": (set(base) - {names[0], names[16]}) | {names[30], names[32], names[33],
                                                         names[34]}}  # 32
    rows = []
    decisions = []
    times = {p: 5.0 + i for i, p in enumerate(names)}
    times[names[32]] = 120.0  # in E's band (90, 180]
    unknown_e = {names[35], names[36], names[37]}
    unknown_p = {names[38]}
    rep = {names[1], names[2], names[3]}
    for p in names:
        decisions.append((p, 180, 1 if p in rep else 0, "disagree" if p in rep else "-"))
        for arm, solved in sets.items():
            passes = (1, 2, 3) if p in rep else (1,)
            for k in passes:
                sol = p in solved
                if p == names[1] and arm == "E" and k == 2:
                    sol = False  # 2 of 3 still solve
                if p in unknown_e and arm == "E":
                    rows.append(row(p, arm, 180, k, 0, split="j13", status="Unsatisfiable",
                                    verdict="Unknown", lane="equals", end_class="proved"))
                    continue
                if p in unknown_p and arm in ("P0", "P1", "PD"):
                    rows.append(row(p, arm, 180, k, 0, split="j13", status="Unsatisfiable",
                                    verdict="Unknown", lane="equals", end_class="proved"))
                    continue
                rows.append(row(p, arm, 180, k, sol, split="j13",
                                wall_s=times[p] if sol else 179.5,
                                end_class=None if sol else "memory-guard"))
    d = os.path.join(tmp, "j13run")
    write_rows(d, rows, decisions)
    return d, names, sets


class Compare(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()

    def tearDown(self):
        for root, dirs, files in os.walk(self.tmp):
            for f in files:
                os.chmod(os.path.join(root, f), 0o600)
        shutil.rmtree(self.tmp)

    def test_compare_cells_known(self):
        d, names, sets = synthetic_j13(self.tmp)
        runs, dec = stats.load_runs([d])
        cells, inc = stats.build_cells(runs, dec, "180", ["E", "P0", "P1", "PD"])
        self.assertEqual(inc, {"E": 0, "P0": 0, "P1": 0, "PD": 0})
        probs = sorted(cells["E"])
        self.assertEqual(len(probs), 40)
        r = stats.compare_cells(cells["E"], cells["P1"], probs, "180", resamples=2000)
        # E solves base - {GRP000, LCL000} + {LAT002, LAT004, LAT005, LAT006(=names[34])}
        self.assertEqual(r["total_a"], 32)
        self.assertEqual(r["total_b"], 31)
        self.assertEqual((r["gains"], r["losses"], r["net"]), (3, 2, 1))
        self.assertEqual(r["domains"]["GRP"], (-1, 16))
        self.assertEqual(r["domains"]["LCL"], (-1, 12))
        self.assertEqual(r["domains"]["LAT"], (3, 12))
        self.assertEqual((r["best_domain"], r["net_drop_best"]), ("LAT", -2))
        self.assertEqual(r["band_a"], 1)
        self.assertEqual(r["band_b"], 0)
        self.assertEqual((r["unknown_a"], r["unknown_b"]), (3, 1))
        self.assertEqual(dict(r["unknown_lanes_a"]), {"equals": 3})
        self.assertEqual(len(r["table"]), 5)
        self.assertEqual(r["ends_b"]["memory-guard"], 40 - 31 - 1)
        self.assertEqual((r["ends_b"]["unknown"], r["ends_a"]["unknown"]), (1, 3))
        self.assertNotIn("proved", r["ends_a"])

    def test_nl_choice(self):
        d, names, sets = synthetic_j13(self.tmp)
        runs, dec = stats.load_runs([d])
        cells, _ = stats.build_cells(runs, dec, "180", ["E", "P0", "P1", "PD"])
        probs = sorted(cells["E"])
        nl, tied, totals = stats.choose_nl(["P0", "P1", "PD"], cells, probs, "E", "180",
                                           2000, stats.DEFAULT_SEED)
        self.assertEqual(totals, {"P0": 30, "P1": 31, "PD": 30})
        self.assertEqual((nl, tied), ("P1", ["P1"]))

    def test_nl_tie_is_conservative(self):
        # P1 and PD both solve 20 of 40.  E = P1 plus one problem (1 gain, 0
        # losses: CI lower bound 0); against PD, E has 6 gains and 5 losses
        # (same net +1, far wider CI): the tie goes to PD, the stricter.
        names = ["GRP%03d-1" % i for i in range(40)]
        p1 = set(names[:20])
        pd = set(names[5:25])
        e = p1 | {names[30]}
        rows = []
        for p in names:
            rows.append(row(p, "P1", 180, 1, p in p1))
            rows.append(row(p, "PD", 180, 1, p in pd))
            rows.append(row(p, "E", 180, 1, p in e))
        dd = os.path.join(self.tmp, "tie")
        write_rows(dd, rows)
        runs, dec = stats.load_runs([dd])
        cells, _ = stats.build_cells(runs, dec, "180", ["E", "P1", "PD"])
        nl, tied, totals = stats.choose_nl(["P1", "PD"], cells, sorted(names), "E", "180",
                                           2000, stats.DEFAULT_SEED)
        self.assertEqual(totals, {"P1": 20, "PD": 20})
        self.assertEqual((nl, sorted(tied)), ("PD", ["P1", "PD"]))
        r = stats.compare_cells(cells["E"], cells["PD"], sorted(names), "180")
        self.assertEqual((r["gains"], r["losses"]), (6, 5))

    def test_incomplete_repeat(self):
        rows = [row("GRP001-1", "E", 180, 1, 1), row("GRP001-1", "P0", 180, 1, 0),
                row("GRP002-1", "E", 180, 1, 1), row("GRP002-1", "P0", 180, 1, 1)]
        d = os.path.join(self.tmp, "inc")
        write_rows(d, rows, [("GRP001-1", 180, 1, "disagree"), ("GRP002-1", 180, 0, "-")])
        runs, dec = stats.load_runs([d])
        cells, inc = stats.build_cells(runs, dec, "180", ["E", "P0"])
        self.assertEqual(inc, {"E": 1, "P0": 1})
        self.assertEqual(sorted(cells["E"]), ["GRP002-1"])

    def test_two_w_band(self):
        rows = []
        for i in range(6):
            p = "GRP%03d-1" % i
            rows.append(row(p, "E", 56, 1, i < 3, wall_s=40.0 if i == 0 else 10.0))
            rows.append(row(p, "P0", 56, 1, i < 2, wall_s=10.0))
            rows.append(row(p, "E", 112, 1, i < 5, wall_s=80.0))
            rows.append(row(p, "P0", 112, 1, i < 2, wall_s=10.0))
        d = os.path.join(self.tmp, "band")
        write_rows(d, rows)
        rc, out = run_cli(["compare", "--rows", d, "--pair", "E,P0", "--wall", "56",
                           "--resamples", "500"])
        self.assertEqual(rc, 0)
        self.assertIn("BAND wall=56 arm=E solves_in_(28,56]=1 misses_solved_at_112=2/3", out)
        self.assertIn("BAND wall=56 arm=P0 solves_in_(28,56]=0 misses_solved_at_112=0/4", out)
        self.assertIn("COMPARE wall=56 E-P0 n=6 gains=1 losses=0 net=+1", out)

    def test_seal(self):
        d, names, sets = synthetic_j13(self.tmp)
        sealed = os.path.join(self.tmp, "sealed")
        f30 = os.path.join(self.tmp, "c30.txt")
        f28 = os.path.join(self.tmp, "c28.txt")
        with open(f30, "w") as fp:
            fp.write("\n".join(names[:20]) + "\n")
        with open(f28, "w") as fp:
            fp.write("\n".join(names[15:30]) + "\n")
        common = ["--rows", d, "--pair", "E,NL", "--pair", "E,PD", "--nl", "P0,P1,PD",
                  "--delta", "2", "--casc30", f30, "--casc28", f28, "--resamples", "500"]
        with self.assertRaises(SystemExit):
            run_cli(["compare"] + common)                       # J13 rows need the seal
        with self.assertRaises(SystemExit):
            run_cli(["compare", "--aggregates-only"] + common)  # and a label
        rc, out = run_cli(["compare", "--aggregates-only", "--label", "L1test",
                           "--sealed-dir", sealed] + common)
        self.assertEqual(rc, 0)
        for n in names:
            self.assertNotIn(n, out)
        self.assertNotIn("DISAGREEMENTS", out)
        self.assertNotIn("SUBSET", out)
        self.assertNotIn("END ", out)
        self.assertIn("NL wall=180 = P1 (totals P0:30 P1:31 PD:30)", out)
        self.assertIn("INCONCLUSIVE wall=180 E-NL (E-P1) no (|3-1|=2 vs delta 2)", out)
        self.assertIn("INCONCLUSIVE wall=180 E-PD no (|3-1|=2 vs delta 2)", out)
        self.assertIn("COMPARE wall=180 E-NL (E-P1) n=40 gains=3 losses=2 net=+1", out)
        self.assertIn("COMPARE wall=180 E-PD n=40 gains=4 losses=2 net=+2", out)
        self.assertIn("DOMAINS wall=180 E-NL (E-P1) GRP:-1/16 LAT:+3/12 LCL:-1/12", out)
        self.assertIn("DROP-BEST wall=180 E-NL (E-P1) best_domain=LAT net_without_it=-2", out)
        self.assertIn("BAND wall=180 arm=E solves_in_(90,180]=1 misses_solved_at_360=no-2W-run",
                      out)
        self.assertIn("TOTALS wall=180 n=40 E: solutions=32 szs=35 | P1: solutions=31 szs=32", out)
        files = os.listdir(sealed)
        self.assertEqual(len(files), 1)
        path = os.path.join(sealed, files[0])
        self.assertFalse(os.stat(path).st_mode & (stat.S_IWUSR | stat.S_IWGRP | stat.S_IWOTH))
        with gzip.open(path, "rt") as fp:
            body = fp.read()
        self.assertIn("## disagreements wall=180 E-NL (E-P1)", body)
        self.assertIn("## j13 subsets", body)
        self.assertIn(names[0], body)
        self.assertIn("casc28-only", body)
        self.assertIn("SEALED per-problem tables -> %s" % path, out)
        rc, out2 = run_cli(["compare", "--look", "L2"] + common)
        self.assertIn("DISAGREEMENTS wall=180 E-NL (E-P1) n=5", out2)
        self.assertIn("SUBSET wall=180 E-NL (E-P1) minus10 n=12", out2)

    def test_reads_archives(self):
        import tarfile
        d, names, sets = synthetic_j13(self.tmp)
        tgz = os.path.join(self.tmp, "L1_1.tgz")
        with tarfile.open(tgz, "w:gz") as tf:
            tf.add(d, arcname="out_L1_1")
        a_runs, a_dec = stats.load_runs([tgz])
        b_runs, b_dec = stats.load_runs([d])
        self.assertEqual(len(a_runs), len(b_runs))
        self.assertEqual(a_dec, b_dec)
        with self.assertRaises(SystemExit):
            stats.load_runs([tgz, d])  # the same rows twice

    def test_dev_guard(self):
        d = os.path.join(self.tmp, "dev")
        write_rows(d, [row("GRP001-1", "E", 180, 1, 1), row("GRP001-1", "P0", 180, 1, 1)])
        rc, out = run_cli(["compare", "--rows", d, "--pair", "E,P0", "--resamples", "200"])
        self.assertEqual(rc, 0)
        self.assertIn("net=+0", out)

    def test_aa_pass_specs_and_pairing(self):
        rows = []
        for i in range(12):
            p = "RNG%03d-1" % i
            rows.append(row(p, "P0", 180, 1, 1, split="j13", host="podA"))
            rows.append(row(p, "P0", 180, 2, i >= 3, split="j13",
                            host="podB" if i == 0 else "podA"))
        rows[1] = row("RNG000-1", "P0", 180, 2, 0, split="j13", host="podB")
        rows[1][run_arms.ROW_COLUMNS.index("bin_sha")] = "cd" * 32
        d = os.path.join(self.tmp, "aa")
        write_rows(d, rows)
        rc, out = run_cli(["compare", "--rows", d, "--pair", "P0@1,P0@2", "--aggregates-only",
                           "--label", "L0aa", "--sealed-dir", os.path.join(self.tmp, "s"),
                           "--resamples", "300"])
        self.assertIn("COMPARE wall=180 P0@1-P0@2 n=12 gains=3 losses=0 net=+3", out)
        self.assertIn("PAIRING wall=180 problems_with_runs_on_several_pods=1", out)
        self.assertIn("BINARY arm=P0 abababababab:23 cdcdcdcdcdcd:1 WARNING: several binaries", out)
        self.assertNotIn("RNG0", out)

    def test_inconclusive_rule(self):
        rows = []
        for i in range(10):
            p = "GRP%03d-1" % i
            rows.append(row(p, "E", 180, 1, i < 5))
            if i < 4:
                rows.append(row(p, "P0", 180, 1, 0, status="Unsatisfiable", verdict="Unknown"))
            else:
                rows.append(row(p, "P0", 180, 1, i < 6))
        d = os.path.join(self.tmp, "inc2")
        write_rows(d, rows)
        rc, out = run_cli(["compare", "--rows", d, "--pair", "E,P0", "--delta", "3",
                           "--resamples", "200"])
        self.assertIn("UNKNOWN wall=180 arm=P0 unknown=4 by_lane=twee:4", out)
        self.assertIn("INCONCLUSIVE wall=180 E-P0 YES (|0-4|=4 vs delta 3)", out)
        rc, out = run_cli(["compare", "--rows", d, "--pair", "E,P0", "--delta", "4",
                           "--resamples", "200"])
        self.assertIn("INCONCLUSIVE wall=180 E-P0 no", out)


class DeltaCmd(unittest.TestCase):
    def test_g02(self):
        tmp = tempfile.mkdtemp()
        try:
            rows = []
            for i in range(30):
                p = "LAT%03d-1" % i
                for w, dis in ((180, 5), (56, 13)):
                    rows.append(row(p, "P0", w, 1, 1, split="j13"))
                    rows.append(row(p, "P0", w, 2, 0 if i < dis else 1, split="j13"))
            write_rows(os.path.join(tmp, "l0"), rows)
            rc, out = run_cli(["delta", "--rows", os.path.join(tmp, "l0")])
            self.assertEqual(rc, 1)
            self.assertIn("AA wall=56 arm=P0 problems=30 pass1_solutions=30 pass2_solutions=17 "
                          "D=13 D_szs=13 unknown=0/0 delta=4 G0.2=FAIL", out)
            self.assertIn("AA wall=180 arm=P0 problems=30 pass1_solutions=30 pass2_solutions=25 "
                          "D=5 D_szs=5 unknown=0/0 delta=3 G0.2=PASS", out)
            for i in range(30):
                self.assertNotIn("LAT%03d-1" % i, out)
            runs, _ = stats.load_runs([os.path.join(tmp, "l0")])
            self.assertEqual(stats.delta_by_wall(runs, "P0", ["180", "56"]),
                             {"180": (5, 3, 30), "56": (13, 4, 30)})
        finally:
            shutil.rmtree(tmp)


class Wdep(unittest.TestCase):
    def make(self, tmp, plan, official):
        """plan: wall -> {problem: pod time or None}."""
        offp = os.path.join(tmp, "official.tsv")
        with open(offp, "w") as fp:
            fp.write("problem\tVampire---5.0.1\tFindProof---0.1\n")
            for p, t in official.items():
                fp.write("%s\t+1.00\t%s\n" % (p, ("+%.2f" % t) if t is not None else "TMO"))
        rows = []
        for w, sol in plan.items():
            for p, t in sol.items():
                if t is None:
                    rows.append(row(p, "SUB", w, 1, 0, split="j13", wall_s=w - 0.4,
                                    verdict="-"))
                else:
                    rows.append(row(p, "SUB", w, 1, 0, split="j13", wall_s=t,
                                    status="Unsatisfiable", verdict="-"))
        write_rows(os.path.join(tmp, "cal"), rows)
        return offp, os.path.join(tmp, "cal")

    def test_choice_and_fallback(self):
        tmp = tempfile.mkdtemp()
        try:
            probs = ["GRP%03d-1" % i for i in range(20)]
            official = {p: (3.0 * (i + 1) if i < 12 else None) for i, p in enumerate(probs)}
            pod = {p: (t / 3.0 if t is not None else None) for p, t in official.items()}
            plan = {}
            plan[45] = dict(pod, **{probs[10]: None, probs[11]: None})       # agreement 0.90
            plan[53] = dict(pod)                                             # exact
            plan[60] = dict(pod, **{probs[0]: None, probs[19]: 30.0})        # 0.90
            plan[70] = dict(pod, **{probs[19]: 30.0})                        # 0.95, total 13
            offp, cal = self.make(tmp, plan, official)
            rc, out = run_cli(["wdep", "--rows", cal, "--official", offp])
            self.assertEqual(rc, 0)
            self.assertIn("OFFICIAL FindProof---0.1 solved=12 of 20", out)
            self.assertIn("WALL 45 covered=20/20 total=10 (diff -2) szs=10 empty_blocks=0 "
                          "agreement=0.9000", out)
            self.assertIn("WALL 70 covered=20/20 total=13 (diff +1) szs=13 empty_blocks=0 "
                          "agreement=0.9500", out)
            self.assertIn("G0.3 PASS: WDEP = 53 s (agreement 1.0000, total 12", out)
            # No wall qualifies: c = 3 exactly from the 70 s run (official >= 1 s).
            plan2 = {w: dict(pod, **{probs[i]: None for i in range(4)}) for w in (45, 53, 60, 70)}
            tmp2 = tempfile.mkdtemp()
            try:
                offp2, cal2 = self.make(tmp2, plan2, official)
                rc, out = run_cli(["wdep", "--rows", cal2, "--official", offp2])
                self.assertEqual(rc, 1)
                self.assertIn("G0.3 FAIL: no wall qualifies; WDEP = 180 / c = 60.0 s, UNCALIBRATED "
                              "(c = 3.000", out)
            finally:
                shutil.rmtree(tmp2)
        finally:
            shutil.rmtree(tmp)

    def test_floor_excludes_startup(self):
        official = {"A%03d-1" % i: 0.5 for i in range(5)}
        official.update({"B%03d-1" % i: 6.0 for i in range(5)})
        runs = []
        for p, t in official.items():
            pod_t = 0.5 if t < 1 else 2.0
            runs.append(stats.Run(dict(zip(run_arms.ROW_COLUMNS,
                                           row(p, "SUB", 70, 1, 0, split="j13", wall_s=pod_t,
                                               status="Unsatisfiable", verdict="-")))))
        res = stats.wdep_analysis(runs, official, "SUB")
        q = res["per_wall"]["70"]
        self.assertAlmostEqual(q["c_floor"], 3.0)
        self.assertAlmostEqual(q["c_all"], math.sqrt(3.0))
        self.assertEqual(q["n_floor"], 5)
        self.assertAlmostEqual(res["fallback"]["wdep"], 60.0)


class RunArmsParsers(unittest.TestCase):
    PROOF = ("% SZS status Unsatisfiable for GRP001-1\n"
             "% SZS output start CNFRefutation for GRP001-1\n"
             "cnf(a, axiom, f(X) = X, file('/x/GRP001-1.p', a)).\n"
             "% SZS output end CNFRefutation for GRP001-1\n")

    def test_status_and_proof(self):
        self.assertEqual(run_arms.parse_szs_status(self.PROOF), "Unsatisfiable")
        self.assertEqual(run_arms.parse_szs_status(""), "NoOutput")
        self.assertEqual(run_arms.proof_block(self.PROOF), (True, "CNFRefutation"))
        cut = self.PROOF.rsplit("% SZS output end", 1)[0]
        self.assertEqual(run_arms.proof_block(cut), (False, "CNFRefutation"))
        self.assertEqual(run_arms.proof_block("% SZS status Timeout for x\n"), (False, ""))
        self.assertEqual(run_arms.proof_records(self.PROOF), 1)
        empty = ("% SZS output start CNFRefutation for x\n"
                 "% goal 0: no single-NF chain extractable\n% SZS output end CNFRefutation for x\n")
        self.assertEqual(run_arms.proof_block(empty), (True, "CNFRefutation"))
        self.assertEqual(run_arms.proof_records(empty), 0)
        self.assertEqual(run_arms.solution_flag("Unsatisfiable", True, "CNFRefutation", 1,
                                                "VerifiedGood"), 1)
        for args in (("Unsatisfiable", True, "CNFRefutation", 0, "VerifiedGood"),
                     ("Unsatisfiable", True, "CNFRefutation", 1, "Unknown"),
                     ("Unsatisfiable", True, "Proof", 1, "VerifiedGood"),
                     ("Unsatisfiable", False, "", 1, "VerifiedGood"),
                     ("GaveUp", True, "CNFRefutation", 1, "VerifiedGood")):
            self.assertEqual(run_arms.solution_flag(*args), 0, args)

    def test_winner_and_stop(self):
        err = "FindProof: schedule stopped at the wall (179.5s); a slice had decided\n" \
              "% FindProof schedule winner: tgt-sel\n"
        self.assertEqual(run_arms.parse_winner(err), "tgt-sel")
        self.assertTrue(run_arms.stopped_at_wall(err))
        self.assertEqual(run_arms.parse_winner("nothing\n"), "")

    def test_classify_lane(self):
        c = run_arms.classify_lane
        self.assertEqual(c("=> PROVED\n   goal=x  steps=10  rules=3  cps=40  max_cps=40  1.0s\n",
                           False, True), ("proved", "PROVED", 10))
        self.assertEqual(c("=> ABORTED (memory guard / abort hook)\n", False, False)[0],
                         "memory-guard")
        self.assertEqual(c("thvm fatal: heap_alloc: from-space exhausted [phase=gc]\n",
                           False, False)[0], "memory-guard")
        self.assertEqual(c("thvm fatal: bad tag\n", False, False)[0], "crash")
        self.assertEqual(c("=> QUEUE_EMPTY (saturated, no proof)\n", False, False)[0],
                         "saturated")
        self.assertEqual(c("  [wall cap 177s reached]\n=> RUNNING (wall cap)\n", False, False)[0],
                         "wall")
        self.assertEqual(c("=> RUNNING (wall cap)\n", False, False)[0], "step-cap")
        self.assertEqual(c("ordering=kbo  step_cap=500000  wall_cap=177s\n"
                           "  step  499999  rules=10     cps=20       5.0s\n=> TIMEOUT (step cap)\n",
                           False, False)[0], "step-cap")
        self.assertEqual(c("ordering=lpo  step_cap=500000  wall_cap=177s\n"
                           "  step    1200  rules=10     cps=20       5.0s\n=> TIMEOUT (step cap)\n",
                           False, False), ("wall", "TIMEOUT (step cap)", 1200))
        self.assertEqual(c("FindProof: fof slice: input has no fof() record\n", False, False)[0],
                         "immediate-exit")
        self.assertEqual(c("FindProof: horn-back slice: no proof within budget\n", False, False)[0],
                         "saturated")
        self.assertEqual(c("FindProof: OOM in the clausifier\n", False, False)[0], "memory-guard")
        self.assertEqual(c("  step     100  rules=10     cps=20       5.0s\n", True, False)[0],
                         "wall")
        self.assertEqual(c("", False, True)[0], "stopped-by-winner")
        self.assertEqual(c("garbage\n", False, False), ("unknown", "garbage", None))

    def test_classify_run(self):
        f = run_arms.classify_run
        self.assertEqual(f(0, "", {}, "twee"), "proved")
        self.assertEqual(f(124, "", {"wall": 3}, ""), "alarm")
        self.assertEqual(f(142, "", {}, ""), "alarm")
        self.assertEqual(f(2, "", {}, ""), "crash")
        self.assertEqual(f(1, "FindProof: schedule stopped at the wall (179.5s)\n",
                           {"memory-guard": 2}, ""), "wall")
        self.assertEqual(f(1, "", {"memory-guard": 1, "saturated": 3, "immediate-exit": 20}, ""),
                         "memory-guard")
        self.assertEqual(f(1, "", {"saturated": 3, "immediate-exit": 20}, ""), "saturated")
        self.assertEqual(f(1, "", {"immediate-exit": 46}, ""), "immediate-exit")
        self.assertEqual(f(1, "", {}, ""), "unknown")
        self.assertEqual(f(0, "", {}, "", "Unsatisfiable"), "proved")  # no winner line (July build)

    def test_clock_gated_and_schedule_source(self):
        g = run_arms.is_clock_gated
        self.assertTrue(g("--tgt-sel=1 --lrs --preset=none"))
        self.assertTrue(g("--cp-set-interreduce --cp-set-ir-budget=5"))
        self.assertTrue(g("--fof-slice=2 --fol-lrs=1"))
        self.assertFalse(g("--fol-lrs=0 --lrs=0"))
        self.assertFalse(g("--cp-set-interreduce --tgt=1"))
        main_c = os.path.join(REPO, "tools", "atp_cli", "main.c")
        if not os.path.exists(main_c):
            self.skipTest("no main.c")
        lanes = run_arms.parse_schedule_source(run_arms.read_text(main_c))
        self.assertEqual(len(lanes), 46)
        gated = sorted(k for k, v in lanes.items() if g(v))
        self.assertEqual(gated, sorted(["equals-twee", "fof-wall", "unfold-tgt-awr", "twee",
                                        "twee-freq", "tgt-sel", "otter-lpo", "otter-lpo-cmin",
                                        "otter-ladr", "twee-raw"]))

    def test_repeat_decision(self):
        known = {"twee", "wm", "tgt-sel"}
        gated = {"twee", "tgt-sel"}
        self.assertEqual(run_arms.repeat_decision({"E": (1, "wm"), "P0": (1, "wm")}, gated, known),
                         (0, "-"))
        self.assertEqual(run_arms.repeat_decision({"E": (1, "wm"), "P0": (0, "")}, gated, known),
                         (1, "disagree"))
        self.assertEqual(run_arms.repeat_decision({"E": (1, "twee"), "P0": (1, "wm")}, gated,
                                                  known), (1, "clock-gated:twee"))
        self.assertEqual(run_arms.repeat_decision({"E": (0, "enigma-x"), "P0": (0, "")}, gated,
                                                  known), (1, "clock-gated:enigma-x"))

    def test_order_and_shard(self):
        names = ["GRP%03d-1" % i for i in range(10)] + ["LAT%03d-1" % i for i in range(7)]
        shards = [run_arms.shard_select(names, k, 4) for k in range(1, 5)]
        self.assertEqual(sorted(sum(shards, [])), sorted(names))
        self.assertEqual(sum(len(s) for s in shards), len(names))
        self.assertTrue(max(len(s) for s in shards) - min(len(s) for s in shards) <= 1)
        k1 = run_arms.order_key(20261001, "GRP001-1", 1, "180", "E")
        self.assertEqual(k1, run_arms.order_key(20261001, "GRP001-1", 1, "180", "E"))
        self.assertNotEqual(k1, run_arms.order_key(20261001, "GRP001-1", 1, "180", "P0"))
        self.assertEqual(run_arms.fmt_num(180.0), "180")
        self.assertEqual(run_arms.fmt_num(53.5), "53.5")
        self.assertEqual(run_arms.rss_cap_for(64 * 1024 * 1024), 3500)
        self.assertIsNone(run_arms.rss_cap_for(128 * 1024 * 1024))

    def test_select_reference(self):
        res = []
        for name, ts in (("A", [5.0, 5.1, 5.2]), ("B", [5.0, 6.0, 7.0]), ("C", [1.0, 1.0, 1.0]),
                         ("D", [10.0, 10.1, 10.0]), ("E", [3.0, 3.0, None])):
            for t in ts:
                res.append({"name": name, "path": name + ".p", "ok": t is not None,
                            "wall_s": t if t is not None else 120.0, "ref_s": None})
        chosen = run_arms.select_reference(res, 10, 2.0, 60.0, 0.10)
        self.assertEqual([c[1] for c in chosen], ["D", "A"])


if __name__ == "__main__":
    unittest.main()
