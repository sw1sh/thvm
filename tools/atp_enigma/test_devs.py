#!/usr/bin/env python3
"""Known-answer tests for devs.py (plan M2 deliverable 4, amendment A25):
the source lanes, the source jobs, the candidate rows of synthetic version-2
exports (extract), the selection and the problem files (filter), and the
band and G2.5 readings over synthetic run_arms.py rows (stage, band, gate).

Synthetic data only; nothing here runs a prover.  Pods only, like every
test of this repository (needs numpy, as np0_extract.py does):
  python3 tools/atp_enigma/test_devs.py -v
"""
import contextlib
import hashlib
import io
import os
import re
import shutil
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import numpy as np  # noqa: E402

import devs  # noqa: E402
import np0_extract as nx  # noqa: E402

MAIN_C = os.path.join(HERE, "..", "atp_cli", "main.c")
SOURCE_LANES = ["wm", "flatten", "flatten-bare", "twee", "twee-deep", "twee-freq", "tgt-sel",
                "otter-lpo", "otter-lpo-cmin", "otter-ladr", "mnf-gj-amw", "encompass", "bare",
                "ifeq", "twee-raw", "bare-raw", "flatten-bare-raw", "wm-gj", "lazy-gj", "bare-gj"]

AXIOMS = """% group axioms
cnf(assoc,axiom,
    f(f(X,Y),Z) = f(X,f(Y,Z)) ).
cnf(left_id,axiom,
    f(e,X) = X ).
cnf(left_inv,axiom,
    f(i(X),X) = e ).
"""
PROBLEMS = {
    # A positive negated_conjecture unit: kept, so TST001-1 is its own system.
    "TST001-1": "% Status : Unsatisfiable\ninclude('Axioms/TST001-0.ax').\n"
                "cnf(hyp,negated_conjecture,\n    f(a,a) = e ).\n"
                "cnf(goal,negated_conjecture,\n    f(a,b) != f(b,a) ).\n",
    "TST002-1": "include('Axioms/TST001-0.ax').\n"
                "cnf(goal,negated_conjecture,\n    f(i(i(a)),b) != f(a,b) ).\n",
    "TST003-1": "include('Axioms/TST001-0.ax').\ncnf(goal,negated_conjecture,\n    i(e) != e ).\n",
    # Inline axioms, a goal with role axiom, a symbol named like a fresh
    # constant and a clause named devs_goal.
    "TST004-1": "cnf(comm,axiom,\n    g(X,Y) = g(Y,X) ).\n"
                "cnf(devs_goal,axiom,\n    g(devsk0,X) = X ). % (a comment)\n"
                "cnf(c2,axiom,\n    g(a,b) != g(b,a) ).\n",
    "TST900-1": "include('Axioms/TST900-0.ax').\ncnf(goal,negated_conjecture,\n    h(a) != a ).\n",
    # A train problem whose goal one candidate repeats (filter: known).
    "TST100-1": "include('Axioms/TST001-0.ax').\ncnf(goal,negated_conjecture,\n    f(i(e),c) != c ).\n",
}
J13 = {
    # Candidate B's goal under other names (filter: j13).
    "TSTJ1-1": "include('Axioms/X.ax').\ncnf(g,negated_conjecture,\n    mult(inv(x),mult(x,y)) != y ).\n",
    "TSTJ2-1": "cnf(h,negated_conjecture,\n    k(c) = c ).\ncnf(g,negated_conjecture,\n    k(k(c)) != c ).\n",
}

# Symbol labels of the synthetic exports.
E, I, F, A, B, G, SKF, LONG, DV = 10, 11, 12, 13, 14, 15, 16, 17, 18
SYMBOLS = [(E, b"e", 0, 0), (I, b"i", 1, 0), (F, b"f", 2, 0), (A, b"a", 0, 0), (B, b"b", 0, 0),
           (G, b"g", 2, 0), (SKF, b"sKf0", 0, 2), (LONG, b"averylongname", 0, 1),
           (DV, b"devsk0", 0, 0)]


def v(k):
    return ("v", k)


def fn(label, *kids):
    return (label, list(kids))


def varint(x):
    out = bytearray()
    while True:
        b = x & 0x7F
        x >>= 7
        if x:
            out.append(b | 0x80)
        else:
            out.append(b)
            return bytes(out)


def enc(t):
    if t[0] == "v":
        return varint(t[1] << 1 | 1)
    out = varint(t[0] << 1) + varint(len(t[1]))
    for k in t[1]:
        out += enc(k)
    return out


def rec(dtype, tag, **fields):
    r = np.zeros(1, dtype=dtype)
    r["tag"] = ord(tag)
    if dtype is nx.REC:
        r["pos_len"] = 255
        r["trace"] = nx.NONE
        r["pa"] = nx.NONE
        r["pb"] = nx.NONE
    for k, val in fields.items():
        r[k] = val
    return r.view(nx.REC)


def q_records(step, lhs, rhs, kind=1, flags=nx.Q_ORIENTED, nocontent=False):
    el, er = enc(lhs), enc(rhs)
    buf = el + er
    n = (len(buf) + 61) // 62
    out = [rec(nx.REC, "Q", kind=kind, flags=flags | (nx.Q_NOCONTENT if nocontent else 0),
               pos_len=0 if nocontent else n, seq=step, step=step, trace=step, pri=0,
               pa=len(el), pb=len(er))]
    if not nocontent:
        for k in range(n):
            c = np.zeros(1, dtype=nx.CREC)
            c["tag"] = ord("q")
            c["index"] = k
            part = buf[62 * k:62 * (k + 1)]
            c["bytes"][0, :len(part)] = list(part)
            out.append(c.view(nx.REC))
    return out


def session(lane, n_sel, rules, closed=True, version=2):
    """A session: 'B', the symbols, one 'S' and one 'P' per step, the rules
    ('Q' + 'q') at their steps, and 'E' when closed."""
    out = [rec(nx.HREC, "B", kind=version if version >= 2 else 0, lane=lane, lane_len=len(lane),
               pri=nx.NONE, pb=nx.NONE)]
    if version >= 2:
        for label, name, arity, flags in SYMBOLS:
            out.append(rec(nx.YREC, "Y", label=label, name=name, name_len=len(name),
                           arity=arity, flags=flags))
    by_step = {}
    for r in rules:
        by_step.setdefault(r[0], []).append(r)
    for step in range(1, n_sel + 1):
        out.append(rec(nx.REC, "P", kind=3, seq=step, step=step - 1, trace=100 + step))
        out.append(rec(nx.REC, "S", kind=3, seq=step, step=step, trace=100 + step))
        for r in by_step.get(step, ()):
            out.extend(q_records(*r))
    if closed:
        out.append(rec(nx.REC, "E", kind=0xff, seq=0, step=n_sel))
    return out


def write_export(path, sessions):
    np.concatenate([x for s in sessions for x in s]).tofile(path)


def big(depth):
    t = v(0)
    for _ in range(depth):
        t = fn(I, t)
    return t


# The rules of the synthetic runs: (step, lhs, rhs[, kind, flags, nocontent]).
R_A = (fn(F, v(0), fn(F, fn(I, v(0)), v(1))), v(1))               # valid
R_B = (fn(F, fn(I, v(0)), fn(F, v(0), v(1))), v(1))               # valid; J13
R_D = (fn(F, v(0), fn(F, v(1), v(2))), fn(F, v(1), fn(F, v(0), v(2))))   # valid, unoriented
R_E = (fn(F, fn(I, fn(I, v(0))), v(1)), fn(F, v(0), v(1)))        # TST002-1's own goal
R_F = (fn(F, fn(I, fn(F, v(0), v(1))), fn(F, v(0), fn(F, v(1), v(2)))), v(2))   # valid
R_G = (fn(F, fn(I, fn(E)), v(0)), v(0))                            # TST100-1's goal
R_H = (fn(F, fn(A), fn(F, fn(A), v(0))), v(0))                     # TST001-1, uses its hyp's a
R_I = (fn(G, fn(G, v(0), v(1)), v(2)), fn(G, v(2), fn(G, v(1), v(0))))   # TST004-1
TWEE_002 = [
    (2, fn(F, fn(E), fn(F, fn(E), v(0))), v(0)),                     # first half
    (6,) + R_A,
    (6,) + R_B,
    (7, fn(F, v(0), fn(E)), v(0)),                                   # 4 symbols
    (7, fn(F, fn(F, v(0), v(1)), v(2)), fn(F, v(0), fn(F, v(1), v(2)))),   # assoc itself
    (7, fn(F, fn(E), fn(I, v(0))), fn(I, v(0))),                     # left_id instance
    (8, fn(G, v(0), v(1)), fn(G, v(1), v(0))),                       # g: not TST002-1's
    (8, fn(F, fn(SKF), fn(F, v(0), v(1))), fn(F, v(0), v(1))),       # fresh symbol
    (8,) + R_D + (1, 0),
    (9, v(5), fn(F, fn(I, v(3)), fn(F, v(3), v(5)))),                # B again, flipped
    (9, fn(F, v(0), v(0)), v(0), 3),                                 # compose: not born
    (9, fn(F, v(0), v(1)), v(1), 1, nx.Q_ORIENTED, True),            # no content
    (9, big(64), v(0)),                                              # 66 symbols
    (9,) + R_E,
    (9, fn(F, fn(A), fn(F, v(0), v(1))), fn(F, v(0), v(1))),         # a: goal-only
    (9, fn(F, fn(LONG), fn(F, v(0), v(1))), fn(F, v(0), v(1))),      # truncated name
]


def pool_problems_text():
    rows = [("TST001-1", "heldout"), ("TST002-1", "heldout"), ("TST003-1", "heldout"),
            ("TST004-1", "heldout"), ("TST900-1", "heldout"), ("TST100-1", "train")]
    out = "".join("%s\tProblems/TST/%s.p\t%s\ttptp\n" % (n, n, s) for n, s in rows)
    return out + "TSTJ1-1\tUEQ/TSTJ1-1.p\tj13\tj13local\n"


@contextlib.contextmanager
def quiet():
    with contextlib.redirect_stdout(io.StringIO()) as out, contextlib.redirect_stderr(io.StringIO()):
        yield out


class World:
    """A temporary pool root, J13 directory, corpus and the exports."""

    def __init__(self, d):
        self.d = d
        self.root = os.path.join(d, "pool")
        os.makedirs(os.path.join(self.root, "Axioms"))
        os.makedirs(os.path.join(self.root, "Problems", "TST"))
        with open(os.path.join(self.root, "Axioms", "TST001-0.ax"), "w") as fp:
            fp.write(AXIOMS)
        with open(os.path.join(self.root, "Axioms", "TST900-0.ax"), "w") as fp:
            fp.write("cnf(n,axiom,\n    h(X) != X ).\n")
        for name, text in PROBLEMS.items():
            with open(self.path(name), "w") as fp:
                fp.write(text)
        self.j13 = os.path.join(d, "j13")
        os.makedirs(self.j13)
        for name, text in J13.items():
            with open(os.path.join(self.j13, name + ".p"), "w") as fp:
                fp.write(text)
        self.corpus = os.path.join(d, "corpus")
        for part, name, goal in (("TRAIN-X", "TSTX01-1-10", "ifeq(a,b,c,d) != d"),
                                 ("DEV-X", "TSTX02-1-10", "ifeq(c,c,a,b) != a")):
            base = os.path.join(self.corpus, part)
            os.makedirs(os.path.join(base, "Problems", "TST"))
            with open(os.path.join(base, "pool.tsv"), "w") as fp:
                fp.write("%s\tProblems/TST/%s.p\tUnsatisfiable\tCNF\t%s\n" % (name, name, part))
            with open(os.path.join(base, "Problems", "TST", name + ".p"), "w") as fp:
                fp.write("cnf(goal,negated_conjecture,\n    %s ).\n" % goal)
        self.pool_problems = os.path.join(d, "problems.tsv")
        with open(self.pool_problems, "w") as fp:
            fp.write(pool_problems_text())
        self.hx = os.path.join(d, "hx")
        os.makedirs(self.hx)
        self.cand = os.path.join(d, "cand")
        self.jobs = [("TST001-1", "twee"), ("TST002-1", "twee"), ("TST002-1", "wm"),
                     ("TST003-1", "wm"), ("TST004-1", "bare"), ("TST900-1", "bare")]
        write_export(self.hxp("TST002-1", "twee"),
                     [session(b"twee", 10, TWEE_002), session(b"twee", 4, [(3,) + R_F], version=1)])
        write_export(self.hxp("TST002-1", "wm"),
                     [session(b"wm", 8, [(5,) + R_A, (6,) + R_F], closed=False)])
        write_export(self.hxp("TST003-1", "wm"), [session(b"wm", 4, [(3,) + R_A, (3,) + R_G])])
        write_export(self.hxp("TST001-1", "twee"), [session(b"twee", 2, [(1,) + R_H])])
        write_export(self.hxp("TST004-1", "bare"), [session(b"bare", 2, [(1,) + R_I])])
        self.jobs_file = os.path.join(d, "source_jobs.tsv")
        with open(self.jobs_file, "w") as fp:
            for n, l in self.jobs:
                fp.write("%s\tProblems/TST/%s.p\theldout\t%s\n" % (n, n, l))

    def path(self, name):
        return os.path.join(self.root, "Problems", "TST", name + ".p")

    def hxp(self, name, lane):
        return os.path.join(self.hx, "%s__%s.hx" % (name, lane))

    def cand_path(self, name, lane):
        return os.path.join(self.cand, "%s__%s.cand.tsv" % (name, lane))

    def extract(self, name, lane, *extra):
        with quiet() as out:
            rc = devs.main(["extract", "--root", self.root, "--source", name,
                            "--path", "Problems/TST/%s.p" % name, "--lane", lane,
                            "--hx", self.hxp(name, lane), "--out", self.cand_path(name, lane)]
                           + list(extra))
        return rc, out.getvalue()

    def extract_all(self):
        for n, l in self.jobs:
            rc, out = self.extract(n, l)
            assert rc == 0, out

    def filter(self, out, *extra):
        with quiet() as text:
            rc = devs.main(["filter", "--cand", self.cand, "--jobs", self.jobs_file,
                            "--pool-root", self.root, "--pool-problems", self.pool_problems,
                            "--corpus-root", self.corpus, "--j13-dir", self.j13, "--out", out]
                           + list(extra))
        return rc, text.getvalue()


def ground(src, lhs, rhs):
    """The goal key of a synthetic rule, as extract computes it."""
    names = {label: (name.decode(), arity, flags) for label, name, arity, flags in SYMBOLS}
    order = devs.var_order(lhs, rhs)
    gmap = {x: ("%s%d" % (src.fresh, k), ()) for k, x in enumerate(order)}
    lg = devs.named(fn_t(lhs), names, src.symtab, gmap)
    rg = devs.named(fn_t(rhs), names, src.symtab, gmap)
    return devs.literal_key(lg, rg)


def fn_t(t):
    """A synthetic term in decode_term's shape (kids as tuples)."""
    if t[0] == "v":
        return t
    return (t[0], tuple(fn_t(k) for k in t[1]))


def read_cand(path):
    meta, rows = devs.read_cand_file(path)
    return meta, rows


class Lanes(unittest.TestCase):
    def test_flags(self):
        t = devs.input_transforms
        self.assertEqual(t("--tgt=1 --preset=none --horn=0"), ["--tgt"])
        self.assertEqual(t("--tgt-sel=1 --tgt-nongoal=4 --lrs --tgt-sel-exit"), [])
        self.assertEqual(t("--tgt=0"), [])
        self.assertEqual(t("--def-unfold=2 --tgt=1 --tgt-fold=1"), ["--def-unfold", "--tgt"])
        self.assertEqual(t("--equals-goal=2 --equals-goal=0"), [])
        self.assertEqual(t("--narrow-queue"), ["--narrow-queue"])
        self.assertEqual(t("--flatten-goal --canon-input=0 --ifeq-guard=2 --otter"), [])
        self.assertEqual(t("--fof-slice=2 --fol-age-ratio=1"), ["--fof-slice"])
        self.assertEqual(t("--horn-cd=2 --fof-sat=0"), ["--horn-cd"])

    def test_schedule(self):
        """The rule over SCHEDULE[] of tools/atp_cli/main.c gives A25's 20 lanes."""
        table, inside = [], False
        with open(MAIN_C) as fp:
            for line in fp:
                if line.startswith("static const SchedSlice SCHEDULE[]"):
                    inside = True
                elif inside and line.startswith("};"):
                    break
                elif inside:
                    m = re.match(r'\s*\{"([^"]+)",\s*"([^"]*)",', line)
                    if m:
                        table.append((m.group(1), m.group(2)))
        self.assertEqual(len(table), 46)
        self.assertEqual(devs.source_lanes(table), SOURCE_LANES)

    def test_jobs(self):
        with tempfile.TemporaryDirectory() as d:
            probs = os.path.join(d, "problems.tsv")
            with open(probs, "w") as fp:
                for n, s in (("AAA001-1", "heldout"), ("BBB001-1", "heldout"), ("AAA002-1", "train"),
                             ("CCC001-1", "heldout"), ("DDD001-1", "heldout"), ("EEE001-1", "j13")):
                    fp.write("%s\tProblems/%s/%s.p\t%s\ttptp\n" % (n, n[:3], n, s))
            dev_t = os.path.join(d, "dev_t.tsv")
            with open(dev_t, "w") as fp:
                fp.write("problem\tsplit\tcondition\nBBB001-1\theldout\tfastest>10s\n")
            train_t = os.path.join(d, "train_t.tsv")
            with open(train_t, "w") as fp:
                fp.write("problem\tcondition\nAAA002-1\tfastest>10s\n")
            lanes = os.path.join(d, "lanes.txt")
            with open(lanes, "w") as fp:
                fp.write("wm\t\nequals\t--equals-goal=2 --fof-sat=0\ntwee\t--simpler-join\n"
                         "tgt\t--tgt=1 --preset=none\n")
            out = os.path.join(d, "out")
            with quiet():
                rc = devs.main(["jobs", "--problems", probs, "--dev-t", dev_t, "--train-t", train_t,
                                "--lane-table", lanes, "--slots", "2", "--out", out])
            self.assertEqual(rc, 0)
            with open(os.path.join(out, "sources.tsv")) as fp:
                self.assertEqual([l.split("\t")[0] for l in fp.read().splitlines()],
                                 ["AAA001-1", "CCC001-1", "DDD001-1"])
            with open(os.path.join(out, "source_lanes.txt")) as fp:
                self.assertEqual(fp.read(), "wm\ntwee\n")
            with open(os.path.join(out, "source_jobs.tsv")) as fp:
                self.assertEqual(len(fp.read().splitlines()), 6)
            slots = []
            for k in (1, 2):
                with open(os.path.join(out, "slots", "slot_%d.tsv" % k)) as fp:
                    rows = [l.split("\t") for l in fp.read().splitlines()]
                slots.append(sorted({r[0] for r in rows}))
                self.assertEqual([devs.job_order(r[0], r[3]) for r in rows],
                                 sorted(devs.job_order(r[0], r[3]) for r in rows))
            self.assertEqual(slots, [["AAA001-1", "DDD001-1"], ["CCC001-1"]])
            with open(os.path.join(out, "jobs.txt")) as fp:
                for line in fp:
                    h, f = line.split()
                    with open(os.path.join(out, f), "rb") as g:
                        self.assertEqual(hashlib.sha256(g.read()).hexdigest(), h)


class Sources(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.w = World(self.tmp)

    def tearDown(self):
        shutil.rmtree(self.tmp)

    def src(self, name):
        return devs.Source(self.w.root, "Problems/TST/%s.p" % name)

    def test_spans(self):
        text = ("% head (not a record\ninclude('Axioms/A(1).ax').\n"
                "cnf(c1, axiom, f(X) = g(X) ). /* ) */\ncnf('c 2',negated_conjecture,\n"
                "    ( f(a) != b ) ).\n")
        spans = devs.tptp_spans(text)
        self.assertEqual([k for k, _s in spans], ["include", "cnf", "cnf"])
        self.assertEqual(spans[0][1], "include('Axioms/A(1).ax').")
        self.assertEqual(spans[2][1], "cnf('c 2',negated_conjecture,\n    ( f(a) != b ) ).")

    def test_kept_records(self):
        s1 = self.src("TST001-1")
        self.assertEqual(s1.kept, ["include('Axioms/TST001-0.ax').",
                                   "cnf(hyp,negated_conjecture,\n    f(a,a) = e )."])
        self.assertEqual(s1.includes, ["Axioms/TST001-0.ax"])
        self.assertEqual(s1.symtab, {"f": 2, "e": 0, "i": 1, "a": 0})
        s2, s3 = self.src("TST002-1"), self.src("TST003-1")
        self.assertEqual(s2.axkey, s3.axkey)
        self.assertNotEqual(s1.axkey, s2.axkey)
        self.assertEqual(s2.symtab, {"f": 2, "e": 0, "i": 1})
        s4 = self.src("TST004-1")
        self.assertEqual(len(s4.kept), 2)
        self.assertNotIn("c2", "".join(s4.kept))
        self.assertEqual(s4.fresh, "ddevsk")
        self.assertEqual(s4.goal_name, "devs_goal_1")
        self.assertEqual((s2.fresh, s2.goal_name), ("devsk", "devs_goal"))
        with self.assertRaises(devs.Unsupported):
            self.src("TST900-1")

    def test_axkey_renaming(self):
        alt = os.path.join(self.w.root, "Problems", "TST", "TST005-1.p")
        with open(alt, "w") as fp:
            fp.write("cnf(x3,axiom, mul(inv(A),A) = one ).\ncnf(x1,axiom, mul(mul(A,B),C) = "
                     "mul(A,mul(B,C)) ).\ncnf(x2,axiom, mul(one,A) = A ).\n"
                     "cnf(goal,negated_conjecture, mul(c,d) != mul(d,c) ).\n")
        self.assertEqual(self.src("TST005-1").axkey, self.src("TST002-1").axkey)

    def test_instance(self):
        s = self.src("TST002-1")
        c = lambda n: (n, ())  # noqa: E731
        f = lambda x, y: ("f", (x, y))  # noqa: E731
        i = lambda x: ("i", (x,))  # noqa: E731
        self.assertTrue(s.is_instance(f(c("e"), i(c("k0"))), i(c("k0"))))
        self.assertTrue(s.is_instance(c("e"), f(i(f(c("k0"), c("k1"))), f(c("k0"), c("k1")))))
        self.assertFalse(s.is_instance(f(i(c("e")), c("k0")), c("k0")))
        self.assertFalse(s.is_instance(f(i(c("k0")), c("k1")), c("e")))

    def test_keys(self):
        k = devs.literal_key
        c = lambda n: (n, ())  # noqa: E731
        f = lambda n, *a: (n, tuple(a))  # noqa: E731
        x, y = ("?X",), ("?Y",)
        base = k(f("f", c("a"), c("b")), f("f", c("b"), c("a")))
        self.assertEqual(base, k(f("g", c("u"), c("w")), f("g", c("w"), c("u"))))
        self.assertEqual(k(f("f", c("a"), c("b")), c("b")), k(c("q"), f("h", c("p"), c("q"))))
        self.assertNotEqual(base, k(f("f", c("a"), c("a")), f("f", c("a"), c("a"))))
        self.assertEqual(k(f("f", x, y), y), k(f("g", y, x), x))
        self.assertNotEqual(k(f("f", x, c("a")), c("a")), k(f("f", c("b"), c("a")), c("a")))
        keys = devs.goal_keys_of(self.w.path("TST004-1"))
        self.assertEqual(keys, {k(f("g", c("a"), c("b")), f("g", c("b"), c("a")))})
        jk = devs.goal_keys_of(os.path.join(self.w.j13, "TSTJ2-1.p"))
        self.assertEqual(len(jk), 2)

    def test_problem_text(self):
        s = self.src("TST004-1")
        row = dict(source="TST004-1", lane="bare", session="0", birth_step="1", n_sel="2",
                   rule="g(X0,X1) = X1", size="4", goal="g(ddevsk0,ddevsk1) != ddevsk1")
        text = s.problem_text(row)
        self.assertTrue(text.startswith("% DEV-S candidate"))
        self.assertIn("cnf(comm,axiom,\n    g(X,Y) = g(Y,X) ).\n", text)
        self.assertIn("cnf(devs_goal,axiom,\n    g(devsk0,X) = X ).\n", text)
        self.assertNotIn("g(a,b)", text)
        self.assertTrue(text.endswith("cnf(devs_goal_1,negated_conjecture,\n"
                                      "    g(ddevsk0,ddevsk1) != ddevsk1 ).\n"))


class Extract(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.w = World(self.tmp)

    def tearDown(self):
        shutil.rmtree(self.tmp)

    def test_counters_and_rows(self):
        rc, out = self.w.extract("TST002-1", "twee")
        self.assertEqual(rc, 0, out)
        meta, rows = read_cand(self.w.cand_path("TST002-1", "twee"))
        self.assertEqual(meta["status"], "ok")
        want = dict(sessions=2, v1=1, branch=0, unclosed=0, lane_mismatch=0, born=15, late=14,
                    nocontent=1, size=2, foreign=4, trivial=0, repeat=1, distinct=6, walked=6,
                    instance=2, kept=4)
        self.assertEqual({k: int(meta[k]) for k in want}, want)
        self.assertEqual(meta["truncated"], "0")
        s = devs.Source(self.w.root, "Problems/TST/TST002-1.p")
        self.assertEqual(meta["src_sha"], s.sha)
        self.assertEqual(meta["axkey"], s.axkey)
        keys = {ground(s, *r): n for n, r in (("A", R_A), ("B", R_B), ("D", R_D), ("E", R_E))}
        self.assertEqual(sorted(keys[r["goal_key"]] for r in rows), ["A", "B", "D", "E"])
        self.assertEqual([r["rank"] for r in rows], sorted(r["rank"] for r in rows))
        by = {keys[r["goal_key"]]: r for r in rows}
        self.assertEqual(by["A"]["goal"], "f(devsk0,f(i(devsk0),devsk1)) != devsk1")
        self.assertEqual(by["A"]["rule"], "f(X0,f(i(X0),X1)) = X1")
        self.assertEqual((by["A"]["birth_step"], by["A"]["n_sel"], by["A"]["size"]), ("6", "10", "7"))
        self.assertEqual(by["D"]["oriented"], "0")
        self.assertEqual(by["B"]["oriented"], "1")
        self.assertEqual(by["A"]["uid"], by["A"]["rank"][:16])
        self.assertEqual(by["A"]["rank"], devs.rank_of(s.axkey, by["A"]["goal_key"]))

    def test_unclosed_session_and_chunks(self):
        for chunk in ("2", "3", "1048576"):
            rc, out = self.w.extract("TST002-1", "wm", "--chunk", chunk)
            self.assertEqual(rc, 0, out)
            meta, rows = read_cand(self.w.cand_path("TST002-1", "wm"))
            self.assertEqual((meta["unclosed"], meta["late"], meta["kept"]), ("1", "2", "2"))
            self.assertEqual(sorted(r["n_sel"] for r in rows), ["8", "8"])

    def test_top_truncates(self):
        rc, _out = self.w.extract("TST002-1", "twee", "--top", "2")
        self.assertEqual(rc, 0)
        meta, rows = read_cand(self.w.cand_path("TST002-1", "twee"))
        self.assertEqual((len(rows), meta["truncated"], meta["top"]), (2, "1", "2"))

    def test_missing_export_and_unsupported(self):
        os.unlink(self.w.hxp("TST004-1", "bare"))
        rc, _out = self.w.extract("TST004-1", "bare")
        self.assertEqual(rc, 0)
        meta, rows = read_cand(self.w.cand_path("TST004-1", "bare"))
        self.assertEqual((meta["status"], meta["sessions"], meta["hx_bytes"], rows), ("ok", "0", "-1", []))
        rc, _out = self.w.extract("TST900-1", "bare")
        self.assertEqual(rc, 0)
        meta, rows = read_cand(self.w.cand_path("TST900-1", "bare"))
        self.assertEqual((meta["status"], rows), ("unsupported", []))

    def test_error(self):
        os.unlink(self.w.path("TST003-1"))
        rc, _out = self.w.extract("TST003-1", "wm")
        self.assertEqual(rc, 2)
        meta, _rows = read_cand(self.w.cand_path("TST003-1", "wm"))
        self.assertEqual(meta["status"], "error")

    def test_fresh_prefix(self):
        rc, _out = self.w.extract("TST004-1", "bare")
        self.assertEqual(rc, 0)
        _meta, rows = read_cand(self.w.cand_path("TST004-1", "bare"))
        self.assertEqual([r["goal"] for r in rows],
                         ["g(g(ddevsk0,ddevsk1),ddevsk2) != g(ddevsk2,g(ddevsk1,ddevsk0))"])


class Filter(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.w = World(self.tmp)
        self.w.extract_all()

    def tearDown(self):
        shutil.rmtree(self.tmp)

    def ranks(self):
        s2 = devs.Source(self.w.root, "Problems/TST/TST002-1.p")
        return {n: devs.rank_of(s2.axkey, ground(s2, *r)) for n, r in
                (("A", R_A), ("D", R_D), ("F", R_F))}

    def test_selection(self):
        out = os.path.join(self.tmp, "sel")
        rc, text = self.w.filter(out, "--per-system", "2", "--cap", "3")
        self.assertEqual(rc, 0, text)
        self.assertIn("dropped: j13=1 known=2 dup=2 cap=1 total=1", text)
        self.assertIn("J13 goal keys 3 from 2 files", text)
        self.assertNotIn("TSTJ", text)
        man = devs.read_manifest(os.path.join(out, "candidates.tsv"))
        self.assertEqual([m["candidate"] for m in man], ["TST001-1-S1", "TST002-1-S1", "TST004-1-S1"])
        ranks = self.ranks()
        first = min(ranks, key=ranks.get)
        by = {m["candidate"]: m for m in man}
        self.assertEqual(by["TST002-1-S1"]["rank"], ranks[first])
        self.assertEqual(by["TST002-1-S1"]["lane"], "twee" if first != "F" else "wm")
        for m in man:
            p = os.path.join(out, m["path"])
            with open(p, "rb") as fp:
                self.assertEqual(hashlib.sha256(fp.read()).hexdigest(), m["sha"])
        with open(os.path.join(out, by["TST001-1-S1"]["path"])) as fp:
            t1 = fp.read()
        self.assertIn("include('Axioms/TST001-0.ax').", t1)
        self.assertIn("cnf(hyp,negated_conjecture,", t1)
        self.assertNotIn("f(a,b) != f(b,a)", t1)
        self.assertTrue(t1.endswith("cnf(devs_goal,negated_conjecture,\n"
                                    "    f(a,f(a,devsk0)) != devsk0 ).\n"))
        self.assertTrue(os.path.isfile(os.path.join(out, "Axioms", "TST001-0.ax")))
        with open(os.path.join(out, "problems.tsv")) as fp:
            self.assertEqual(fp.read().splitlines()[0],
                             "TST001-1-S1\tProblems/TST/TST001-1-S1.p\tdevs")
        with open(os.path.join(out, "files.sha256")) as fp:
            listed = {ln.split()[1] for ln in fp}
        self.assertEqual(listed, {m["path"] for m in man} | {"Axioms/TST001-0.ax"})
        drops = [ln.rstrip("\n").split("\t") for ln in open(os.path.join(out, "drops.tsv"))][1:]
        dup = sorted((d[2], d[3]) for d in drops if d[4] == "dup")
        self.assertEqual(dup, [("TST002-1", "wm"), ("TST003-1", "wm")])

    def test_no_cap(self):
        out = os.path.join(self.tmp, "sel")
        rc, text = self.w.filter(out)
        self.assertEqual(rc, 0, text)
        self.assertIn("dropped: j13=1 known=2 dup=2 cap=0 total=0", text)
        man = devs.read_manifest(os.path.join(out, "candidates.tsv"))
        ranks = self.ranks()
        s2 = [m for m in man if m["source"] == "TST002-1"]
        self.assertEqual([m["rank"] for m in s2], sorted(ranks.values()))
        self.assertEqual([m["candidate"] for m in s2], ["TST002-1-S1", "TST002-1-S2", "TST002-1-S3"])
        self.assertIn("TST004-1-S1", {m["candidate"] for m in man})
        rc, _text = self.w.filter(out)
        self.assertEqual(rc, 2)

    def test_inexact(self):
        self.w.extract("TST002-1", "twee", "--top", "2")
        self.w.extract("TST002-1", "wm", "--top", "2")
        for n, l in self.w.jobs:
            if (n, l) not in (("TST002-1", "twee"), ("TST002-1", "wm")):
                self.w.extract(n, l, "--top", "2")
        rc, text = self.w.filter(os.path.join(self.tmp, "sel"))
        self.assertEqual(rc, 0, text)
        self.assertIn("inexact TST002-1 twee", text)

    def test_invalid(self):
        os.unlink(self.w.cand_path("TST003-1", "wm"))
        rc, text = self.w.filter(os.path.join(self.tmp, "a"))
        self.assertEqual(rc, devs.EXIT_INVALID)
        self.assertIn("1 jobs have no extract file", text)
        self.w.extract("TST003-1", "wm", "--top", "5")
        rc, text = self.w.filter(os.path.join(self.tmp, "b"))
        self.assertEqual(rc, devs.EXIT_INVALID)
        self.assertIn("different --top", text)
        self.w.extract("TST003-1", "wm")
        with open(self.w.path("TST003-1"), "a") as fp:
            fp.write("% edited\n")
        rc, text = self.w.filter(os.path.join(self.tmp, "c"))
        self.assertEqual(rc, devs.EXIT_INVALID)
        self.assertIn("src_sha", text)


ROW_HEADER = ["problem", "split", "arm", "wall", "pass", "status", "wall_s", "verdict",
              "solution", "bin_sha", "cond"]
# candidate -> wall -> (solution, wall_s); 120 and 360 only where the
# protocol runs them.
MEASURE = {
    "C1-1-S1": {60: (1, 45.0), 180: (1, 46.0)},
    "C2-1-S1": {60: (0, 60.5), 120: (1, 80.0), 180: (1, 100.0)},
    "C3-1-S1": {60: (1, 10.0), 180: (1, 12.0)},
    "C4-1-S1": {60: (0, 60.2), 120: (1, 20.0), 180: (0, 180.4), 360: (1, 200.0)},
    "C5-1-S1": {60: (0, 60.1), 120: (0, 120.3), 180: (0, 180.2), 360: (0, 360.1)},
    "C6-1-S1": {60: (0, 60.3), 120: (1, 31.0), 180: (0, 180.1), 360: (1, 50.0)},
}


def rows_text(cells, sha="b" * 64, cond="maxpar=16 rss=default", verdict=None):
    out = ["\t".join(ROW_HEADER)]
    for name, w, p, sol, t in cells:
        vd = verdict or ("VerifiedGood" if sol else "-")
        out.append("\t".join([name, "devs", "P0", str(w), str(p), "Unsatisfiable" if sol else "Timeout",
                              "%.1f" % t, vd, str(sol), sha, cond]))
    return "\n".join(out) + "\n"


class BandGate(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.man = os.path.join(self.tmp, "candidates.tsv")
        with open(self.man, "w") as fp:
            fp.write("\t".join(devs.MANIFEST_COLUMNS) + "\n")
            for k, name in enumerate(sorted(MEASURE)):
                row = dict(candidate=name, source=name[:5], lane="twee", axkey="ax%d" % (k % 3),
                           birth_step="9", size="7", sha="s%d" % k,
                           path="Problems/CCC/%s.p" % name, session="0", n_sel="10", oriented="1",
                           rank="r", goal_key="g", goal="x != y")
                fp.write("\t".join(row[c] for c in devs.MANIFEST_COLUMNS) + "\n")
        self.probs = os.path.join(self.tmp, "problems.tsv")
        with open(self.probs, "w") as fp:
            fp.write("".join("%s\tProblems/CCC/%s.p\tdevs\n" % (n, n) for n in sorted(MEASURE)))

    def tearDown(self):
        shutil.rmtree(self.tmp)

    def stage_rows(self, walls, drop=None, **kw):
        cells = [(n, w, 1, sol, t) for n, m in sorted(MEASURE.items())
                 for w, (sol, t) in m.items() if w in walls and (n, w) != drop]
        path = os.path.join(self.tmp, "rows_%s.tsv" % "_".join(map(str, walls)))
        with open(path, "w") as fp:
            fp.write(rows_text(cells, **kw))
        return path

    def run_band(self, rows, out="band"):
        with quiet() as text:
            rc = devs.main(["band", "--manifest", self.man, "--out", os.path.join(self.tmp, out)]
                           + [x for r in rows for x in ("--rows", r)])
        return rc, text.getvalue()

    def test_stage(self):
        r60 = self.stage_rows((60, 180))
        nxt = os.path.join(self.tmp, "next.tsv")
        with quiet():
            rc = devs.main(["stage", "--problems", self.probs, "--rows", r60, "--wall", "60", "--out", nxt])
        self.assertEqual(rc, 0)
        with open(nxt) as fp:
            self.assertEqual([l.split("\t")[0] for l in fp.read().splitlines()],
                             ["C2-1-S1", "C4-1-S1", "C5-1-S1", "C6-1-S1"])
        with quiet():
            rc = devs.main(["stage", "--problems", self.probs, "--rows", r60, "--wall", "120",
                            "--out", nxt])
        self.assertEqual(rc, devs.EXIT_INVALID)

    def test_band_and_gate(self):
        rows = [self.stage_rows((60, 180)), self.stage_rows((120,)), self.stage_rows((360,))]
        rc, text = self.run_band(rows)
        self.assertEqual(rc, 0, text)
        self.assertIn("W=60: band=1 band-2w=2 fast=1 erratic=1 miss=1", text)
        self.assertIn("W=180: band=1 band-2w=1 fast=2 erratic=1 miss=1", text)
        self.assertIn("DEV-S: 4 problems; band at 60 s 3", text)
        self.assertIn("band at 180 s 2", text)
        band = os.path.join(self.tmp, "band")
        with open(os.path.join(band, "devs.tsv")) as fp:
            lines = fp.read().splitlines()
        self.assertEqual(lines[0], "#" + "\t".join(devs.DEVS_COLUMNS))
        got = [l.split("\t")[:5] for l in lines[1:]]
        self.assertEqual(got, [["C1-1-S1", "Problems/CCC/C1-1-S1.p", "devs", "1", "0"],
                               ["C2-1-S1", "Problems/CCC/C2-1-S1.p", "devs", "1", "1"],
                               ["C4-1-S1", "Problems/CCC/C4-1-S1.p", "devs", "0", "1"],
                               ["C6-1-S1", "Problems/CCC/C6-1-S1.p", "devs", "1", "0"]])
        with open(os.path.join(band, "devs.tsv"), "rb") as fp:
            self.assertIn(hashlib.sha256(fp.read()).hexdigest() + "  devs.tsv", text)
        names = [g[0] for g in got]
        cells = [(n, w, p, 0 if (n, w, p) == ("C1-1-S1", 60, 2) else 1, 40.0)
                 for n in names for w in (60, 180) for p in (1, 2)]
        aa = os.path.join(self.tmp, "aa.tsv")
        with open(aa, "w") as fp:
            fp.write(rows_text(cells))
        with quiet() as out:
            rc = devs.main(["gate", "--band", band, "--rows", aa])
        self.assertEqual(rc, 1)
        self.assertIn("A/A at 60 s: D = 1 of 4 DEV-S problems (25.00%)", out.getvalue())
        self.assertIn("A/A at 180 s: D = 0 of 4 DEV-S problems (0.00%)", out.getvalue())
        self.assertIn("G2.5: FAIL", out.getvalue())
        with open(aa, "w") as fp:
            fp.write(rows_text(cells, sha="c" * 64))
        with quiet():
            rc = devs.main(["gate", "--band", band, "--rows", aa])
        self.assertEqual(rc, devs.EXIT_INVALID)
        with open(aa, "w") as fp:
            fp.write(rows_text(cells[:-1]))
        with quiet():
            rc = devs.main(["gate", "--band", band, "--rows", aa])
        self.assertEqual(rc, devs.EXIT_INVALID)

    def test_band_invalid(self):
        rows = [self.stage_rows((60, 180)), self.stage_rows((120,)),
                self.stage_rows((360,), drop=("C5-1-S1", 360))]
        rc, text = self.run_band(rows, "b1")
        self.assertEqual(rc, devs.EXIT_INVALID)
        self.assertIn("1 required runs have no row (first: C5-1-S1 at 360 s)", text)
        rows = [self.stage_rows((60, 180)), self.stage_rows((120,), sha="d" * 64),
                self.stage_rows((360,))]
        rc, _text = self.run_band(rows, "b2")
        self.assertEqual(rc, devs.EXIT_INVALID)
        rows = [self.stage_rows((60, 180)), self.stage_rows((120,), verdict="VerifiedBad"),
                self.stage_rows((360,))]
        rc, _text = self.run_band(rows, "b3")
        self.assertEqual(rc, devs.EXIT_VBAD)

    def test_g25(self):
        ok = lambda *a: devs.g25(*a)[1]  # noqa: E731
        self.assertTrue(ok(60, 30, 15, {60: 0.05, 180: 0.0}))
        self.assertFalse(ok(59, 30, 15, {60: 0.0, 180: 0.0}))
        self.assertFalse(ok(60, 29, 15, {60: 0.0, 180: 0.0}))
        self.assertFalse(ok(60, 30, 14, {60: 0.0, 180: 0.0}))
        self.assertFalse(ok(60, 30, 15, {60: 0.0, 180: 0.051}))
        self.assertFalse(ok(60, 30, 15, {60: None, 180: 0.0}))


sys.path.insert(0, os.path.join(HERE, "protocol"))
import run_arms  # noqa: E402


def arms_rows(path, cells, sha="b" * 64, cond="maxpar=16 rss=default", verdict=None):
    """rows_text's cells written by run_arms.py's own writer: its '#'
    header and every ROW_COLUMNS column."""
    run_arms.ensure_header(path, run_arms.ROW_COLUMNS)
    for name, w, p, sol, t in cells:
        row = dict.fromkeys(run_arms.ROW_COLUMNS, "")
        row.update({"problem": name, "split": "devs", "arm": "P0", "wall": run_arms.fmt_num(w),
                    "pass": p, "status": "Unsatisfiable" if sol else "Timeout",
                    "wall_s": "%.2f" % t, "verdict": verdict or ("VerifiedGood" if sol else "-"),
                    "solution": sol, "bin_sha": sha, "cond": cond, "oom_kills": 0})
        run_arms.append_line(path, [row[c] for c in run_arms.ROW_COLUMNS])


class RunArmsRows(BandGate):
    """BandGate's tests on out directories whose rows.tsv run_arms.py wrote:
    the '#' header, every column, and a line a crash cut short."""

    def stage_rows(self, walls, drop=None, **kw):
        cells = [(n, w, 1, sol, t) for n, m in sorted(MEASURE.items())
                 for w, (sol, t) in m.items() if w in walls and (n, w) != drop]
        out = os.path.join(self.tmp, "out_%s" % "_".join(map(str, walls)))
        os.makedirs(out, exist_ok=True)
        path = os.path.join(out, "rows.tsv")
        if os.path.exists(path):
            os.unlink(path)
        arms_rows(path, cells, **kw)
        with open(path, "a") as fp:
            fp.write("C9-1-S1\tdevs\tP0\t60\n")
        return out

    def test_header(self):
        out = self.stage_rows((60, 180))
        with open(os.path.join(out, "rows.tsv")) as fp:
            self.assertEqual(fp.readline().rstrip("\n"), "#" + "\t".join(run_arms.ROW_COLUMNS))
        rows = devs.read_rows([out])
        self.assertEqual(len(rows), 12)
        self.assertEqual((rows[0]["problem"], rows[0]["wall"], rows[0]["pass"]), ("C1-1-S1", "60", "1"))


if __name__ == "__main__":
    unittest.main()
