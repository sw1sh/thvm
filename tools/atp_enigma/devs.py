#!/usr/bin/env python3
"""DEV-S, the manufactured band set (docs/ATP/enigma_portfolio.md, M2
deliverable 4 and gate G2.5; amendment A25 registers every reading here).

A DEV-S problem is a ground goal that a held-out run derived: a rule that
a lane searching the untransformed input oriented in the second half of a
180 s source run on a held-out problem, skolemized and posed against that
problem's axioms.  A candidate stays in DEV-S when P0 solves it in
(W/2, 2W] at WDEP = 60 s or at W180.

Subcommands, in pipeline order:
  lanes    the source lanes of a lane table (FindProof --list-lanes=flags)
  jobs     the source list and the source-run job slots             Mac
  extract  one source run's history export -> candidate rows        pod
  filter   every job's rows -> the selected candidates, their problem
           files, the manifest and run_arms.py's problem list       Mac
  stage    one measurement stage's rows -> the next stage's list    Mac
  band     the measurement rows -> band flags and the DEV-S list    Mac
  gate     G2.5 from the band and the A/A rows                      Mac
extract alone needs numpy (it reads exports through np0_extract.py); the
others process text only.  filter is the only reader of J13 files: it
reads the goals of the problem files in --j13-dir statically (plan
section 1.4), prints only counts and writes nothing derived from them.
Each J13 match drops a candidate; the J13 problem is never named.

Definitions (amendment A25):
- Source lanes: the lanes of the binary's own table whose flags leave
  none of --equals-goal, --narrow-queue, --horn-cd, --horn-back,
  --fof-slice, --def-unfold and --tgt on (A19's reading: the last value
  of a repeated flag wins, a bare flag is 1, a value starting with 0 is
  off).  Those encode the input, rewrite or add axioms, or search outside
  the engine.  Goal naming (--flatten-goal), the canonical presentation
  and the search-control flags leave the axioms as they are.
- Kept records: a source problem's own top-level records, split as
  written; every include() and every record whose literals are all
  positive stay, verbatim and in order; every record with a negative
  literal goes (the goal disequations, whatever their role).  Positive
  negated_conjecture units stay: the engine reads them as equations and
  FindProof's .pr carries no role, so a rule derived from them is a
  consequence of the kept records only.  An included file must hold
  positive cnf records and no include (else the source is unsupported).
- Axiom system: ifeq10_diff.problem_key over the positive records (the
  included ones too), every role read as axiom: blind to variable and
  symbol renaming (per clause), clause order and clause names.
- Candidate: a 'Q' record of kind born (a selected CP oriented into R or
  E) with content, in a version-2 session that is not a fof-split branch,
  whose step is at least n_sel // 2 (n_sel is the session's 'E' step,
  else its largest 'S' step: the second half, as np0_extract.py's
  hindsight labels read it), with 5 to 60 symbols (function symbols,
  constants and variable occurrences of both sides), whose every symbol
  has a 'Y' record that is neither fresh, truncated nor arity-saturated
  and names a symbol of the positive records with that arity, whose sides
  differ, and which is not an instance of a positive unit (one-way
  matching, either orientation).
- Ground goal: the candidate with its variables, in first-occurrence
  order (lhs, then rhs), replaced by fresh constants P0, P1, ... (P =
  devsk, with a 'd' prepended while a symbol of the positive records is P
  followed by digits), written `lhs != rhs` as cnf(devs_goal,
  negated_conjecture, ...) (devs_goal_1, ... if a clause has that name).
  The problem file is the kept records plus that clause, under a comment
  header that names the source, lane, session, birth step and rule.
- Goal key: sha256 of the smaller, over both orientations, of the
  equation's text with every symbol renamed F0, F1, ... and every
  variable V0, V1, ... by first occurrence: blind to variable, symbol and
  ground-constant renaming and to orientation.  A literal's sign is not
  read.
- Rank: sha256("20261001|devs|" + axiom-system key + "|" + goal key); its
  first 16 hex digits are the uid.  Within one job a repeated uid keeps
  its first occurrence (earliest session, then earliest record).  A job
  walks its candidates in rank order, drops the axiom instances, keeps
  the first --top (32) and is truncated when it did not walk them all.
- Filters (filter, in this order):
    j13    the goal key is the key of a literal of a J13 goal clause (a
           negated_conjecture record, or any record with a negative
           literal, of a --j13-dir problem file; includes are not read);
    known  the same against the problems of the pool's train and heldout
           splits and of the synthetic -10 corpus (TRAIN-X, DEV-X), so no
           training or gate problem recurs in DEV-S;
    dup    the rank of an earlier candidate in (source, lane, session,
           birth step) order: the same goal in the same axiom system;
    cap    past the 10th by rank in its axiom system;
    total  past --cap (1000) in the order (position in its axiom system,
           rank), a round robin over the axiom systems.
  A truncated job whose rows hold fewer than 10 survivors of j13 and
  known is inexact (its system's ranking may miss a candidate the job
  never wrote); the summary lists every one.
- Names: <source>-S<k>, k = 1, 2, ... in rank order within the source;
  the file sits beside the source's path, Problems/<DOM>/<name>.p, under
  a root that also holds the included files at their pool paths.
- Band (band): P0's pass-1 rows at 60 and 180 s for every candidate, and
  at 120 s for the misses at 60 s and at 360 s for the misses at 180 s,
  each wall its own run.  At wall W the decisive time is the W run's when
  it is a Solution, else the 2W run's when that one is; the candidate is
  in the W band when the decisive time lies in (W/2, 2W].  A W miss whose
  2W Solution came within W/2 is erratic and out of the band.  DEV-S is
  the union of the two bands.
- G2.5 (gate): at least 60 band problems at WDEP and 30 at W180; at least
  15 axiom systems among the DEV-S problems; and at each of 60 and 180 s
  at most 5% of the DEV-S problems with two A/A passes (run_arms.py
  --passes 2) that differ on Solution.
"""
import argparse
import collections
import glob
import hashlib
import os
import re
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import ifeq10_diff as D  # noqa: E402

SEED = "20261001"
SIZE_MIN = 5
SIZE_MAX = 60
TOP = 32
PER_SYSTEM = 10
CAP = 1000
WALLS = (60, 180)
G25_BAND = {60: 60, 180: 30}
G25_SYSTEMS = 15
G25_AA = 0.05
ARM = "P0"
SPLIT = "devs"
GOAL_NAME = "devs_goal"
FRESH = "devsk"
TRANSFORM_FLAGS = ("--equals-goal", "--narrow-queue", "--horn-cd", "--horn-back",
                   "--fof-slice", "--def-unfold", "--tgt")
NAME_OK = re.compile(r"^(?:[a-z][A-Za-z0-9_]*|[0-9]+)$")
CHUNK = 1 << 20
RULE_BORN = 1
# A node of a 'Q' encoding takes at most 7 bytes (a label varint of 5, an
# arity varint of 2), so a longer encoding holds more than SIZE_MAX nodes.
ENC_MAX = 7 * SIZE_MAX
EXTRACT_VERSION = "1"
CAND_COLUMNS = ["uid", "rank", "source", "lane", "axkey", "session", "n_sel",
                "birth_step", "size", "oriented", "goal_key", "rule", "goal"]
COUNTERS = ["sessions", "v1", "branch", "unclosed", "lane_mismatch", "born", "late",
            "nocontent", "size", "foreign", "trivial", "repeat", "distinct", "walked",
            "instance", "kept"]
MANIFEST_COLUMNS = ["candidate", "source", "lane", "axkey", "birth_step", "size", "sha",
                    "path", "session", "n_sel", "oriented", "rank", "goal_key", "goal"]
DROP_REASONS = ("j13", "known", "dup", "cap", "total")
BAND_COLUMNS = ["candidate", "axkey", "sol60", "t60", "sol120", "t120", "class60",
                "sol180", "t180", "sol360", "t360", "class180"]
DEVS_COLUMNS = ["name", "path", "split", "band60", "band180", "axkey", "sha"]
EXIT_INVALID = 3
EXIT_VBAD = 4


class Unsupported(Exception):
    pass


class Foreign(Exception):
    pass


def sha256_text(text):
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as fp:
        while True:
            b = fp.read(1 << 20)
            if not b:
                break
            h.update(b)
    return h.hexdigest()


def write_text(path, text):
    """Write atomically; return the sha256 of the text."""
    d = os.path.dirname(path)
    if d:
        os.makedirs(d, exist_ok=True)
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8") as fp:
        fp.write(text)
    os.replace(tmp, path)
    return sha256_text(text)


def clean(s):
    return str(s).replace("\t", " ").replace("\n", " ").replace("\r", " ")


def domain(name):
    m = re.match(r"[A-Z]+", name)
    return m.group(0) if m else name


# ----------------------------------------------------------------------------
# Lanes and jobs


def flag_values(flags):
    out = {}
    for tok in flags.split():
        if tok.startswith("--"):
            name, eq, val = tok.partition("=")
            out[name] = val if eq else "1"
    return out


def input_transforms(flags):
    v = flag_values(flags)
    return [f for f in TRANSFORM_FLAGS if f in v and v[f] != "" and v[f][0] != "0"]


def read_lane_table(path):
    rows = []
    with open(path, encoding="utf-8") as fp:
        for line in fp:
            line = line.rstrip("\n")
            if line.strip():
                name, _tab, flags = line.partition("\t")
                rows.append((name.strip(), flags.strip()))
    return rows


def source_lanes(table):
    return [name for name, flags in table if not input_transforms(flags)]


def read_names(path):
    """First column of a list, without a header row ('problem ...') or '#' lines."""
    out = []
    with open(path, encoding="utf-8") as fp:
        for line in fp:
            f = line.rstrip("\n").split("\t")
            if f[0] and f[0] != "problem" and not f[0].startswith("#"):
                out.append(f[0])
    return out


def read_problem_list(path):
    """(name, path, split) rows of a run_arms.py problem list."""
    out = []
    with open(path, encoding="utf-8") as fp:
        for line in fp:
            line = line.rstrip("\n")
            if not line.strip() or line.startswith("#"):
                continue
            f = line.split("\t")
            out.append((f[0], f[1], f[2]))
    return out


def job_order(name, lane):
    return hashlib.sha256((name + "\t" + lane).encode("utf-8")).hexdigest()


def cmd_lanes(a):
    for name in source_lanes(read_lane_table(a.lane_table)):
        print(name)
    return 0


def cmd_jobs(a):
    excl = set(read_names(a.dev_t)) | set(read_names(a.train_t))
    held = [p for p in read_problem_list(a.problems) if p[2] == "heldout"]
    sources = [p for p in held if p[0] not in excl]
    lanes = source_lanes(read_lane_table(a.lane_table))
    if not sources or not lanes:
        print("devs jobs: no sources or no source lanes", file=sys.stderr)
        return 2
    out = a.out
    shas = []
    text = "".join("%s\t%s\t%s\n" % p for p in sources)
    shas.append(("sources.tsv", write_text(os.path.join(out, "sources.tsv"), text)))
    text = "".join(l + "\n" for l in lanes)
    shas.append(("source_lanes.txt", write_text(os.path.join(out, "source_lanes.txt"), text)))
    jobs = [(n, p, s, l) for n, p, s in sources for l in lanes]
    text = "".join("%s\t%s\t%s\t%s\n" % j for j in jobs)
    shas.append(("source_jobs.tsv", write_text(os.path.join(out, "source_jobs.tsv"), text)))
    ordered = sorted(sources, key=lambda p: (domain(p[0]), p[0]))
    for k in range(1, a.slots + 1):
        mine = [p for i, p in enumerate(ordered) if i % a.slots == k - 1]
        sj = sorted(((n, p, s, l) for n, p, s in mine for l in lanes),
                    key=lambda j: job_order(j[0], j[3]))
        rel = os.path.join("slots", "slot_%d.tsv" % k)
        text = "".join("%s\t%s\t%s\t%s\n" % j for j in sj)
        shas.append((rel, write_text(os.path.join(out, rel), text)))
        print("slot %d: %d problems, %d jobs" % (k, len(mine), len(sj)))
    write_text(os.path.join(out, "jobs.txt"), "".join("%s  %s\n" % (h, f) for f, h in shas))
    print("sources %d (heldout %d minus %d in DEV-T or TRAIN-T); lanes %d (%s); jobs %d"
          % (len(sources), len(held), len(held) - len(sources), len(lanes), ", ".join(lanes),
             len(jobs)))
    for f, h in shas:
        print("%s  %s" % (h, f))
    return 0


# ----------------------------------------------------------------------------
# TPTP sources


def tptp_spans(text):
    """The top-level records of a TPTP text, (keyword, span) in order, each
    span running from its keyword to its closing '.'."""
    toks = []
    pos = 0
    while pos < len(text):
        m = D.TOKEN.match(text, pos)
        if m is None:
            raise D.ParseError("bad character %r at offset %d" % (text[pos], pos))
        if m.lastgroup not in ("ws", "comment"):
            toks.append((m.lastgroup, m.group(m.lastgroup), m.start(), m.end()))
        pos = m.end()
    out = []
    i = 0
    while i < len(toks):
        kind, kw, start, _end = toks[i]
        if kind != "word" or i + 1 >= len(toks) or toks[i + 1][1] != "(":
            raise D.ParseError("expected a record at offset %d" % start)
        depth = 0
        j = i + 1
        while j < len(toks):
            v = toks[j][1]
            if v == "(":
                depth += 1
            elif v == ")":
                depth -= 1
                if depth == 0:
                    break
            j += 1
        if j + 1 >= len(toks) or toks[j + 1][1] != ".":
            raise D.ParseError("the record at offset %d does not end with ')' and '.'" % start)
        out.append((kw, text[start:toks[j + 1][3]]))
        i = j + 2
    return out


def span_record(span):
    recs = D.Parser(D.tokens(span)).records(includes=True)
    if len(recs) != 1:
        raise D.ParseError("a span holds %d records" % len(recs))
    return recs[0]


def is_var(t):
    return len(t) == 1


def head(t):
    return None if is_var(t) else (t[0], len(t[1]))


def note_symbols(t, symtab):
    stack = [t]
    while stack:
        x = stack.pop()
        if is_var(x):
            continue
        name, args = x
        old = symtab.get(name)
        if old is None:
            symtab[name] = len(args)
        elif old != len(args):
            symtab[name] = -1
        stack.extend(args)


def match(p, t, sub):
    stack = [(p, t)]
    while stack:
        p, t = stack.pop()
        if is_var(p):
            b = sub.get(p[0])
            if b is None:
                sub[p[0]] = t
            elif b != t:
                return False
        else:
            if is_var(t) or p[0] != t[0] or len(p[1]) != len(t[1]):
                return False
            stack.extend(zip(p[1], t[1]))
    return True


class Source:
    """A source problem: its kept records, symbols, positive units, axiom
    system and the sha256 of its bytes and its included files' bytes."""

    def __init__(self, root, path):
        self.root = root
        self.path = path
        full = os.path.join(root, path)
        with open(full, "rb") as fp:
            raw = fp.read()
        h = hashlib.sha256()
        h.update(path.encode("utf-8") + b"\0" + raw + b"\0")
        self.kept = []
        self.includes = []
        positive = []
        for _kw, span in tptp_spans(raw.decode("utf-8", "replace")):
            rec = span_record(span)
            if rec[0] == D.INCLUDE:
                if rec[2] is not None:
                    raise Unsupported("include with a selection")
                inc = self._resolve(rec[1], full)
                with open(inc, "rb") as fp:
                    inc_raw = fp.read()
                rel = os.path.relpath(inc, root)
                h.update(rel.encode("utf-8") + b"\0" + inc_raw + b"\0")
                for _k2, s2 in tptp_spans(inc_raw.decode("utf-8", "replace")):
                    r2 = span_record(s2)
                    if r2[0] == D.INCLUDE:
                        raise Unsupported("nested include in %s" % rel)
                    if not all(s for s, _l, _r in r2[2]):
                        raise Unsupported("a record of %s has a negative literal" % rel)
                    positive.append(r2)
                self.kept.append(span)
                self.includes.append(rel)
            elif all(s for s, _l, _r in rec[2]):
                self.kept.append(span)
                positive.append(rec)
        self.sha = h.hexdigest()
        self.symtab = {}
        self.units = collections.defaultdict(list)
        clause_names = set()
        for name, _role, lits in positive:
            clause_names.add(name)
            for _s, l, r in lits:
                note_symbols(l, self.symtab)
                note_symbols(r, self.symtab)
            if len(lits) == 1:
                _s, l, r = lits[0]
                self.units[(head(l), head(r))].append((l, r))
        self.axkey = D.problem_key([(n, "axiom", lits) for n, _r, lits in positive])
        self.fresh = FRESH
        while any(re.match(re.escape(self.fresh) + r"[0-9]+$", s) for s in self.symtab):
            self.fresh = "d" + self.fresh
        self.goal_name = GOAL_NAME
        k = 0
        while self.goal_name in clause_names:
            k += 1
            self.goal_name = "%s_%d" % (GOAL_NAME, k)

    def _resolve(self, inc, full):
        for c in (os.path.join(os.path.dirname(full), inc), os.path.join(self.root, inc)):
            if os.path.isfile(c):
                return c
        raise D.ParseError("cannot read include '%s' of %s" % (inc, self.path))

    def is_instance(self, l, r):
        hl, hr = head(l), head(r)
        for a, b, x, y in ((hl, hr, l, r), (hr, hl, r, l)):
            for key in {(a, b), (None, b), (a, None), (None, None)}:
                for s, t in self.units.get(key, ()):
                    sub = {}
                    if match(s, x, sub) and match(t, y, sub):
                        return True
        return False

    def problem_text(self, row):
        head_lines = [
            "% DEV-S candidate: docs/ATP/enigma_portfolio.md M2 deliverable 4, amendment A25"
            " (tools/atp_enigma/devs.py)",
            "%% Source : %s, lane %s, session %s, rule birth step %s of %s selections"
            % (row["source"], row["lane"], row["session"], row["birth_step"], row["n_sel"]),
            "%% Rule   : %s" % row["rule"],
            "%% Size   : %s symbols" % row["size"],
        ]
        parts = ["\n".join(head_lines) + "\n"]
        parts.extend(span + "\n" for span in self.kept)
        parts.append("cnf(%s,negated_conjecture,\n    %s ).\n" % (self.goal_name, row["goal"]))
        return "\n".join(parts)


# ----------------------------------------------------------------------------
# Terms


def tptp(t):
    """TPTP text of a parsed term: ('?X',) variables, (name, args) symbols."""
    out = []
    stack = [t]
    while stack:
        x = stack.pop()
        if isinstance(x, str):
            out.append(x)
        elif is_var(x):
            out.append(x[0][1:])
        elif not x[1]:
            out.append(x[0])
        else:
            out.append(x[0] + "(")
            stack.append(")")
            for j in range(len(x[1]) - 1, -1, -1):
                stack.append(x[1][j])
                if j:
                    stack.append(",")
    return "".join(out)


def _renamed(t, syms, var, out):
    stack = [t]
    while stack:
        x = stack.pop()
        if isinstance(x, str):
            out.append(x)
        elif is_var(x):
            out.append("V%d" % var.setdefault(x[0], len(var)))
        else:
            k = syms.setdefault(x[0], len(syms))
            if not x[1]:
                out.append("F%d" % k)
            else:
                out.append("F%d(" % k)
                stack.append(")")
                for j in range(len(x[1]) - 1, -1, -1):
                    stack.append(x[1][j])
                    if j:
                        stack.append(",")


def literal_key(l, r):
    texts = []
    for a, b in ((l, r), (r, l)):
        syms, var, out = {}, {}, []
        _renamed(a, syms, var, out)
        out.append("=")
        _renamed(b, syms, var, out)
        texts.append("".join(out))
    return sha256_text(min(texts))


def rank_of(axkey, goal_key):
    return sha256_text("%s|devs|%s|%s" % (SEED, axkey, goal_key))


def goal_keys_of(path):
    """Keys of the literals of a problem file's own goal records: those with
    role negated_conjecture or a negative literal (includes not read)."""
    with open(path, encoding="utf-8", errors="replace") as fp:
        recs = D.Parser(D.tokens(fp.read())).records(includes=True)
    keys = set()
    for rec in recs:
        if rec[0] == D.INCLUDE:
            continue
        _name, role, lits = rec
        if role == "negated_conjecture" or not all(s for s, _l, _r in lits):
            for _s, l, r in lits:
                keys.add(literal_key(l, r))
    return keys


# Decoded 'Q' terms (np0_extract.decode_term): ('v', id) or (label, kids).

def enc_nodes(t, cap):
    n = 0
    stack = [t]
    while stack:
        x = stack.pop()
        n += 1
        if n > cap:
            return n
        if x[0] != "v":
            stack.extend(x[1])
    return n


def var_order(*terms):
    seen, order = set(), []
    for t in terms:
        stack = [t]
        while stack:
            x = stack.pop()
            if x[0] == "v":
                if x[1] not in seen:
                    seen.add(x[1])
                    order.append(x[1])
            else:
                stack.extend(reversed(x[1]))
    return order


def named(t, names, symtab, var_map):
    if t[0] == "v":
        return var_map[t[1]]
    ent = names.get(t[0])
    if ent is None:
        raise Foreign()
    name, arity, flags = ent
    kids = t[1]
    if flags & 7 or arity != len(kids) or symtab.get(name) != len(kids) or not NAME_OK.match(name):
        raise Foreign()
    return (name, tuple(named(k, names, symtab, var_map) for k in kids))


# ----------------------------------------------------------------------------
# extract (pod)


def _nx():
    import np0_extract
    return np0_extract


def export_records(path, chunk=CHUNK):
    """The 'B', 'E', 'Y', 'Q' and 'q' records of an export, read in chunks
    (a 180 s export at rate 1 can exceed a gigabyte), the largest 'S' step
    of each session, and the bytes past the last whole record."""
    import numpy as np
    nx = _nx()
    size = os.path.getsize(path) if os.path.isfile(path) else 0
    n = size // nx.REC.itemsize
    tail = size - n * nx.REC.itemsize
    if n == 0:
        return np.zeros(0, dtype=nx.REC), [], tail
    mm = np.memmap(path, dtype=nx.REC, mode="r", shape=(n,))
    keep = np.zeros(256, dtype=bool)
    for c in "BEYQq":
        keep[ord(c)] = True
    parts, smax, cur = [], [], -1
    for c0 in range(0, n, chunk):
        ch = np.array(mm[c0:c0 + chunk])
        tags = ch["tag"]
        edges = [0] + np.flatnonzero(tags == ord("B")).tolist() + [len(ch)]
        for j in range(len(edges) - 1):
            a, z = edges[j], edges[j + 1]
            if j > 0:
                cur += 1
                smax.append(0)
            if cur < 0 or z <= a:
                continue
            seg = ch[a:z]
            st = seg["step"][seg["tag"] == ord("S")]
            if len(st):
                smax[cur] = max(smax[cur], int(st.max()))
        parts.append(ch[keep[tags]])
    del mm
    return np.concatenate(parts), smax, tail


def late_born(recs, b, stop, half, c):
    """(step, flags, lhs, rhs) of the session's born rules from step half on
    whose content fits SIZE_MAX nodes; counts the rest into c."""
    import numpy as np
    nx = _nx()
    r = recs[b:stop]
    born = (r["tag"] == ord("Q")) & (r["kind"] == RULE_BORN)
    c["born"] += int(born.sum())
    late = born & (r["step"] >= half)
    c["late"] += int(late.sum())
    flags = r["flags"].astype(np.int64)
    nocontent = late & ((flags & (nx.Q_TRUNC | nx.Q_NOCONTENT)) != 0)
    c["nocontent"] += int(nocontent.sum())
    enc = r["pa"].astype(np.int64) + r["pb"].astype(np.int64)
    long_ = late & ~nocontent & (enc > ENC_MAX)
    c["size"] += int(long_.sum())
    cont = r.view(nx.CREC)
    for i in np.flatnonzero(late & ~nocontent & ~long_).tolist():
        q = r[i]
        n = int(q["pos_len"])
        span = cont[i + 1:i + 1 + n]
        ok = (len(span) == n and bool(np.all(span["tag"] == ord("q")))
              and np.array_equal(span["index"], np.arange(n)))
        lhs = rhs = None
        if ok:
            ln = int(q["pa"]) + int(q["pb"])
            buf = bytes(span["bytes"].reshape(-1)[:ln])
            try:
                lhs, at = nx.decode_term(buf, 0)
                rhs, at2 = nx.decode_term(buf, at)
                if at != int(q["pa"]) or at2 != ln:
                    lhs = rhs = None
            except ValueError:
                lhs = rhs = None
        if lhs is None:
            c["nocontent"] += 1
            continue
        yield int(q["step"]), int(q["flags"]), lhs, rhs


def extract_job(src, lane, hx, top, chunk=CHUNK):
    """(kept rows, counters, truncated, tail bytes) of one source run."""
    nx = _nx()
    c = collections.Counter({k: 0 for k in COUNTERS})
    recs, smax, tail = export_records(hx, chunk)
    sessions = nx.sessions(recs)
    if len(sessions) != len(smax):
        raise RuntimeError("session count mismatch (%d, %d)" % (len(sessions), len(smax)))
    found = collections.OrderedDict()
    for si, (b, stop, e) in enumerate(sessions):
        c["sessions"] += 1
        version, slane = nx.header(recs, b)
        if version < 2:
            c["v1"] += 1
            continue
        if nx.fof_branch(recs, b) is not None:
            c["branch"] += 1
            continue
        if slane != lane:
            c["lane_mismatch"] += 1
        names = nx.symbols(recs, b, stop)
        if e is None:
            c["unclosed"] += 1
            n_sel = smax[si]
        else:
            n_sel = int(recs[e]["step"])
        for step, flags, lhs, rhs in late_born(recs, b, stop, n_sel // 2, c):
            size = enc_nodes(lhs, SIZE_MAX) + enc_nodes(rhs, SIZE_MAX)
            if size < SIZE_MIN or size > SIZE_MAX:
                c["size"] += 1
                continue
            order = var_order(lhs, rhs)
            try:
                gmap = {v: ("%s%d" % (src.fresh, k), ()) for k, v in enumerate(order)}
                lg = named(lhs, names, src.symtab, gmap)
                rg = named(rhs, names, src.symtab, gmap)
            except Foreign:
                c["foreign"] += 1
                continue
            if lg == rg:
                c["trivial"] += 1
                continue
            gk = literal_key(lg, rg)
            rank = rank_of(src.axkey, gk)
            uid = rank[:16]
            if uid in found:
                c["repeat"] += 1
                continue
            vmap = {v: ("?X%d" % k,) for k, v in enumerate(order)}
            rule = "%s = %s" % (tptp(named(lhs, names, src.symtab, vmap)),
                                tptp(named(rhs, names, src.symtab, vmap)))
            found[uid] = dict(uid=uid, rank=rank, session=si, n_sel=n_sel, birth_step=step,
                              size=size, oriented=int(bool(flags & nx.Q_ORIENTED)),
                              goal_key=gk, rule=rule,
                              goal="%s != %s" % (tptp(lg), tptp(rg)), lg=lg, rg=rg)
    ordered = sorted(found.values(), key=lambda x: x["rank"])
    kept = []
    for row in ordered:
        if len(kept) >= top:
            break
        c["walked"] += 1
        if src.is_instance(row["lg"], row["rg"]):
            c["instance"] += 1
            continue
        kept.append(row)
    c["distinct"] = len(ordered)
    c["kept"] = len(kept)
    return kept, c, int(c["walked"] < len(ordered)), tail


def cand_text(meta, rows):
    line = "#devs-extract\t" + "\t".join("%s=%s" % (k, clean(v)) for k, v in meta)
    out = [line, "\t".join(CAND_COLUMNS)]
    for r in rows:
        out.append("\t".join(clean(r[k]) for k in CAND_COLUMNS))
    return "\n".join(out) + "\n"


def cmd_extract(a):
    meta = [("version", EXTRACT_VERSION), ("status", "ok"), ("source", a.source),
            ("path", a.path), ("lane", a.lane), ("top", a.top)]
    rows = []
    rc = 0
    try:
        src = Source(a.root, a.path)
        meta += [("src_sha", src.sha), ("axkey", src.axkey)]
        hx_bytes = os.path.getsize(a.hx) if os.path.isfile(a.hx) else -1
        kept, c, truncated, tail = extract_job(src, a.lane, a.hx, a.top, a.chunk)
        meta += [("hx_bytes", hx_bytes), ("hx_tail", tail)]
        meta += [(k, c[k]) for k in COUNTERS] + [("truncated", truncated)]
        for r in kept:
            r = dict(r, source=a.source, lane=a.lane, axkey=src.axkey)
            rows.append(r)
        summary = " ".join("%s=%s" % kv for kv in meta[6:] if kv[0] not in ("src_sha", "axkey"))
    except Unsupported as e:
        meta[1] = ("status", "unsupported")
        meta.append(("reason", e))
        summary = "unsupported: %s" % e
    except Exception as e:  # noqa: BLE001 -- recorded in the row file, which filter refuses
        meta[1] = ("status", "error")
        meta.append(("reason", "%s: %s" % (type(e).__name__, e)))
        summary = "error: %s: %s" % (type(e).__name__, e)
        rc = 2
    write_text(a.out, cand_text(meta, rows))
    print("%s %s %s %s" % ("ok" if rc == 0 else "error", a.source, a.lane, summary))
    return rc


# ----------------------------------------------------------------------------
# filter (Mac)


def read_cand_file(path):
    meta, cols, rows = None, None, []
    with open(path, encoding="utf-8") as fp:
        for line in fp:
            line = line.rstrip("\n")
            if line.startswith("#devs-extract"):
                meta = dict(kv.split("=", 1) for kv in line.split("\t")[1:] if "=" in kv)
            elif cols is None:
                cols = line.split("\t")
            elif line:
                rows.append(dict(zip(cols, line.split("\t"))))
    if meta is None or cols != CAND_COLUMNS:
        raise SystemExit("devs filter: %s is not an extract file" % path)
    return meta, rows


def cand_files(paths):
    out = []
    for p in paths:
        if os.path.isdir(p):
            out.extend(glob.glob(os.path.join(p, "**", "*.cand.tsv"), recursive=True))
        else:
            out.append(p)
    return sorted(set(out))


def known_goal_keys(pool_root, pool_problems, corpus_root):
    keys, n = set(), 0
    for name, path, split in read_problem_list(pool_problems):
        if split in ("train", "heldout"):
            keys |= goal_keys_of(os.path.join(pool_root, path))
            n += 1
    for part in ("TRAIN-X", "DEV-X"):
        base = os.path.join(corpus_root, part)
        with open(os.path.join(base, "pool.tsv"), encoding="utf-8") as fp:
            for line in fp:
                f = line.rstrip("\n").split("\t")
                if len(f) >= 2 and f[1]:
                    keys |= goal_keys_of(os.path.join(base, f[1]))
                    n += 1
    return keys, n


def cmd_filter(a):
    if os.path.exists(os.path.join(a.out, "candidates.tsv")):
        print("devs filter: %s already holds a selection" % a.out, file=sys.stderr)
        return 2
    jobs, paths = [], {}
    with open(a.jobs, encoding="utf-8") as fp:
        for ln in fp:
            if ln.strip():
                name, path, _split, lane = ln.rstrip("\n").split("\t")
                jobs.append((name, lane))
                paths[name] = path
    by_job = {}
    problems = []
    for f in cand_files(a.cand):
        meta, rows = read_cand_file(f)
        key = (meta.get("source"), meta.get("lane"))
        if key in by_job:
            problems.append("two extract files for %s %s" % key)
        by_job[key] = (meta, rows)
    jobset = set(jobs)
    missing = [j for j in jobs if j not in by_job]
    extra = sorted(k for k in by_job if k not in jobset)
    errors = sorted(k for k, (m, _r) in by_job.items() if m.get("status") == "error")
    tops = sorted({m.get("top") for m, _r in by_job.values()})
    if missing:
        problems.append("%d jobs have no extract file (first: %s %s)" % ((len(missing),) + missing[0]))
    if extra:
        problems.append("%d extract files are not jobs (first: %s %s)" % ((len(extra),) + extra[0]))
    if errors:
        problems.append("%d extract files hold status=error (first: %s %s)" % ((len(errors),) + errors[0]))
    if len(tops) != 1:
        problems.append("the jobs ran with different --top values: %s" % ", ".join(map(str, tops)))
    sources = {}
    unsupported = set()
    for name in sorted({n for n, _l in jobs}):
        try:
            sources[name] = Source(a.pool_root, paths[name])
        except Unsupported:
            unsupported.add(name)
    for (name, lane), (meta, _rows) in sorted(by_job.items()):
        if (name, lane) not in jobset or meta.get("status") == "error":
            continue
        if (meta.get("status") == "unsupported") != (name in unsupported):
            problems.append("%s %s: the pod and the Mac disagree on support" % (name, lane))
        elif name in sources and meta.get("src_sha") != sources[name].sha:
            problems.append("%s %s: the pod read other source bytes (src_sha)" % (name, lane))
    if problems:
        for p in problems:
            print("INVALID: " + p)
        return EXIT_INVALID
    top = int(tops[0])

    j13, n_j13 = set(), 0
    for p in sorted(glob.glob(os.path.join(a.j13_dir, "*.p"))):
        j13 |= goal_keys_of(p)
        n_j13 += 1
    known, n_known = known_goal_keys(a.pool_root, a.pool_problems, a.corpus_root)

    reason = {}
    rows = []
    for (name, lane), (meta, jrows) in sorted(by_job.items()):
        for r in jrows:
            r["_job"] = (name, lane)
            r["_order"] = (name, lane, int(r["session"]), int(r["birth_step"]))
            rows.append(r)
    for i, r in enumerate(rows):
        if r["goal_key"] in j13:
            reason[i] = "j13"
        elif r["goal_key"] in known:
            reason[i] = "known"
    by_rank = collections.defaultdict(list)
    for i, r in enumerate(rows):
        if i not in reason:
            by_rank[r["rank"]].append(i)
    reps = []
    for rank, idx in by_rank.items():
        idx.sort(key=lambda i: rows[i]["_order"])
        reps.append(idx[0])
        for i in idx[1:]:
            reason[i] = "dup"
    by_sys = collections.defaultdict(list)
    for i in reps:
        by_sys[rows[i]["axkey"]].append(i)
    position = {}
    for axkey, idx in by_sys.items():
        idx.sort(key=lambda i: rows[i]["rank"])
        for k, i in enumerate(idx, 1):
            position[i] = k
            if k > a.per_system:
                reason[i] = "cap"
    within = sorted((i for i in reps if i not in reason), key=lambda i: (position[i], rows[i]["rank"]))
    selected = within[:a.cap]
    for i in within[a.cap:]:
        reason[i] = "total"
    inexact = []
    for (name, lane), (meta, jrows) in sorted(by_job.items()):
        if meta.get("truncated") == "1":
            alive = sum(1 for r in jrows if r["goal_key"] not in j13 and r["goal_key"] not in known)
            if alive < a.per_system:
                inexact.append((name, lane, alive))

    per_source = collections.defaultdict(list)
    for i in selected:
        per_source[rows[i]["source"]].append(i)
    out_rows = []
    written = {}
    for name in sorted(per_source):
        src = sources[name]
        for k, i in enumerate(sorted(per_source[name], key=lambda i: rows[i]["rank"]), 1):
            r = rows[i]
            cand = "%s-S%d" % (name, k)
            rel = os.path.join(os.path.dirname(src.path), cand + ".p")
            written[rel] = write_text(os.path.join(a.out, rel), src.problem_text(r))
            out_rows.append(dict(r, candidate=cand, path=rel, sha=written[rel]))
        for inc in src.includes:
            dst = os.path.join(a.out, inc)
            if inc not in written:
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                shutil.copyfile(os.path.join(a.pool_root, inc), dst)
                written[inc] = sha256_file(dst)
    out_rows.sort(key=lambda r: r["candidate"])
    text = "\t".join(MANIFEST_COLUMNS) + "\n" + "".join(
        "\t".join(clean(r[k]) for k in MANIFEST_COLUMNS) + "\n" for r in out_rows)
    sha_manifest = write_text(os.path.join(a.out, "candidates.tsv"), text)
    text = "".join("%s\t%s\t%s\n" % (r["candidate"], r["path"], SPLIT) for r in out_rows)
    sha_problems = write_text(os.path.join(a.out, "problems.tsv"), text)
    text = "uid\trank\tsource\tlane\treason\n" + "".join(
        "%s\t%s\t%s\t%s\t%s\n" % (rows[i]["uid"], rows[i]["rank"], rows[i]["_job"][0],
                                  rows[i]["_job"][1], reason[i]) for i in sorted(reason))
    sha_drops = write_text(os.path.join(a.out, "drops.tsv"), text)
    text = "".join("%s  %s\n" % (written[p], p) for p in sorted(written))
    sha_files = write_text(os.path.join(a.out, "files.sha256"), text)

    counts = collections.Counter(reason.values())
    systems = collections.Counter(rows[i]["axkey"] for i in selected)
    meta_counts = collections.Counter()
    for meta, _r in by_job.values():
        for k in COUNTERS:
            meta_counts[k] += int(meta.get(k, 0) or 0)
    lines = [
        "devs filter (amendment A25): %d jobs on %d sources (%d unsupported: %s)"
        % (len(jobs), len({n for n, _l in jobs}), len(unsupported), ", ".join(sorted(unsupported)) or "none"),
        "pod counters: " + " ".join("%s=%d" % (k, meta_counts[k]) for k in COUNTERS),
        "jobs truncated at --top %d: %d; inexact: %d%s" % (
            top, sum(1 for m, _r in by_job.values() if m.get("truncated") == "1"), len(inexact),
            "".join("\n  inexact %s %s (%d survivors)" % x for x in inexact)),
        "rows %d; J13 goal keys %d from %d files; known goal keys %d from %d files"
        % (len(rows), len(j13), n_j13, len(known), n_known),
        "dropped: " + " ".join("%s=%d" % (k, counts[k]) for k in DROP_REASONS),
        "selected %d candidates from %d sources in %d axiom systems (--per-system %d, --cap %d)"
        % (len(selected), len(per_source), len(systems), a.per_system, a.cap),
        "per axiom system: " + " ".join("%d:%d" % kv for kv in sorted(collections.Counter(systems.values()).items())),
        "%s  candidates.tsv" % sha_manifest,
        "%s  problems.tsv" % sha_problems,
        "%s  drops.tsv" % sha_drops,
        "%s  files.sha256 (%d files)" % (sha_files, len(written)),
    ]
    write_text(os.path.join(a.out, "summary.txt"), "\n".join(lines) + "\n")
    print("\n".join(lines))
    return 0


# ----------------------------------------------------------------------------
# stage, band, gate (Mac): run_arms.py rows


def read_rows(paths):
    """run_arms.py's rows.tsv as run_arms.read_tsv reads it: the first line
    is the header, '#' first ("#problem<TAB>split..."); a later '#' line,
    and a row whose field count is not the header's (a line a crash cut
    short), are skipped, so a run without its row counts as missing."""
    rows = []
    for p in paths:
        f = os.path.join(p, "rows.tsv") if os.path.isdir(p) else p
        with open(f, encoding="utf-8") as fp:
            first = fp.readline().rstrip("\n")
            header = (first[1:] if first.startswith("#") else first).split("\t")
            for line in fp:
                line = line.rstrip("\n")
                if not line.strip() or line.startswith("#"):
                    continue
                cells = line.split("\t")
                if len(cells) == len(header):
                    rows.append(dict(zip(header, cells)))
    return rows


def wall_key(w):
    return str(int(w)) if float(w) == int(float(w)) else str(w)


def index_rows(rows):
    """(problem, wall, pass) -> row of arm P0; and the problems of those keys
    that have more than one row."""
    idx, dups = {}, set()
    for r in rows:
        if r.get("arm") != ARM:
            continue
        k = (r["problem"], wall_key(r["wall"]), r["pass"])
        if k in idx:
            dups.add(k)
        idx[k] = r
    return idx, dups


def validity(rows):
    """Problems with the rows' run conditions, as printable lines."""
    out = []
    if any(r.get("verdict") == "VerifiedBad" for r in rows):
        out.append(("vbad", "a VerifiedBad run: the line stops until its cause is fixed"))
    shas = sorted({r.get("bin_sha") for r in rows if r.get("arm") == ARM})
    conds = sorted({r.get("cond") for r in rows if r.get("arm") == ARM})
    arms = sorted({r.get("arm") for r in rows})
    splits = sorted({r.get("split") for r in rows})
    if len(shas) > 1:
        out.append(("invalid", "%d binaries: %s" % (len(shas), ", ".join(shas))))
    if len(conds) > 1:
        out.append(("invalid", "%d run conditions: %s" % (len(conds), " | ".join(conds))))
    if arms != [ARM] and rows:
        out.append(("invalid", "arms other than %s: %s" % (ARM, ", ".join(arms))))
    if splits != [SPLIT] and rows:
        out.append(("invalid", "splits other than %s: %s" % (SPLIT, ", ".join(splits))))
    return out, (shas[0] if shas else ""), (conds[0] if conds else "")


def report_validity(problems):
    for kind, text in problems:
        print("%s: %s" % ("VERIFIEDBAD" if kind == "vbad" else "INVALID", text))
    if any(k == "vbad" for k, _t in problems):
        return EXIT_VBAD
    return EXIT_INVALID if problems else 0


def is_solution(row):
    return row is not None and row.get("solution") == "1"


def cmd_stage(a):
    probs = read_problem_list(a.problems)
    rows = read_rows(a.rows)
    problems, _sha, _cond = validity(rows)
    idx, dups = index_rows(rows)
    w = wall_key(a.wall)
    names = {p[0] for p in probs}
    missing = [p[0] for p in probs if (p[0], w, "1") not in idx]
    if dups & {(n, w, "1") for n in names}:
        problems.append(("invalid", "problems with two pass-1 rows at %s s" % w))
    if missing:
        problems.append(("invalid", "%d problems have no pass-1 row at %s s (first: %s)"
                         % (len(missing), w, missing[0])))
    rc = report_validity(problems)
    if rc:
        return rc
    nxt = [p for p in probs if not is_solution(idx[(p[0], w, "1")])]
    write_text(a.out, "".join("%s\t%s\t%s\n" % p for p in nxt))
    print("stage %s s: %d problems, %d Solutions, %d misses -> %s"
          % (w, len(probs), len(probs) - len(nxt), len(nxt), a.out))
    return 0


def classify(w, rw, r2w):
    """(class, in band) at wall w from the w row and the 2w row."""
    if is_solution(rw):
        t = float(rw["wall_s"])
        return ("band", True) if t > w / 2.0 else ("fast", False)
    if is_solution(r2w):
        t = float(r2w["wall_s"])
        return ("band-2w", True) if t > w / 2.0 else ("erratic", False)
    return ("miss", False)


def read_manifest(path):
    with open(path, encoding="utf-8") as fp:
        cols = fp.readline().rstrip("\n").split("\t")
        return [dict(zip(cols, ln.rstrip("\n").split("\t"))) for ln in fp if ln.strip()]


def cell(r):
    if r is None:
        return "-", "-"
    return r.get("solution", "-"), r.get("wall_s", "-")


def cmd_band(a):
    man = read_manifest(a.manifest)
    rows = read_rows(a.rows)
    problems, sha, cond = validity(rows)
    idx, dups = index_rows(rows)
    names = {m["candidate"] for m in man}
    if any(k[0] in names and k[2] == "1" for k in dups):
        problems.append(("invalid", "candidates with two pass-1 rows at one wall"))
    need = []
    for m in man:
        n = m["candidate"]
        for w in WALLS:
            rw = idx.get((n, wall_key(w), "1"))
            if rw is None:
                need.append((n, w))
            elif not is_solution(rw) and (n, wall_key(2 * w), "1") not in idx:
                need.append((n, 2 * w))
    if need:
        problems.append(("invalid", "%d required runs have no row (first: %s at %d s)"
                         % ((len(need),) + need[0])))
    rc = report_validity(problems)
    if rc:
        return rc
    band_lines, devs = [], []
    classes = {w: collections.Counter() for w in WALLS}
    for m in sorted(man, key=lambda x: x["candidate"]):
        n = m["candidate"]
        rec = {"candidate": n, "axkey": m["axkey"]}
        inband = {}
        for w in WALLS:
            rw = idx.get((n, wall_key(w), "1"))
            r2w = None if is_solution(rw) else idx.get((n, wall_key(2 * w), "1"))
            cls, inband[w] = classify(w, rw, r2w)
            classes[w][cls] += 1
            rec["sol%d" % w], rec["t%d" % w] = cell(rw)
            rec["sol%d" % (2 * w)], rec["t%d" % (2 * w)] = cell(r2w)
            rec["class%d" % w] = cls
        band_lines.append("\t".join(rec[k] for k in BAND_COLUMNS))
        if inband[60] or inband[180]:
            devs.append((n, m["path"], SPLIT, int(inband[60]), int(inband[180]), m["axkey"], m["sha"]))
    os.makedirs(a.out, exist_ok=True)
    write_text(os.path.join(a.out, "band.tsv"), "\t".join(BAND_COLUMNS) + "\n"
               + "".join(x + "\n" for x in band_lines))
    sha_devs = write_text(os.path.join(a.out, "devs.tsv"), "#" + "\t".join(DEVS_COLUMNS) + "\n"
                          + "".join("%s\t%s\t%s\t%d\t%d\t%s\t%s\n" % d for d in devs))
    write_text(os.path.join(a.out, "conditions.txt"), "bin_sha=%s\ncond=%s\n" % (sha, cond))
    n60 = sum(d[3] for d in devs)
    n180 = sum(d[4] for d in devs)
    systems = len({d[5] for d in devs})
    lines = ["devs band (amendment A25): %d candidates, binary %s" % (len(man), sha)]
    for w in WALLS:
        lines.append("W=%d: " % w + " ".join("%s=%d" % (k, classes[w][k]) for k in
                                             ("band", "band-2w", "fast", "erratic", "miss")))
    lines += [
        "DEV-S: %d problems; band at 60 s %d (G2.5 needs %d: %s); band at 180 s %d (needs %d: %s)"
        % (len(devs), n60, G25_BAND[60], "PASS" if n60 >= G25_BAND[60] else "FAIL",
           n180, G25_BAND[180], "PASS" if n180 >= G25_BAND[180] else "FAIL"),
        "axiom systems: %d (G2.5 needs %d: %s)" % (systems, G25_SYSTEMS,
                                                   "PASS" if systems >= G25_SYSTEMS else "FAIL"),
        "%s  devs.tsv (the freeze: this sha256 enters the registry before any ENIGMA arm or the A/A pass runs)"
        % sha_devs,
    ]
    write_text(os.path.join(a.out, "summary.txt"), "\n".join(lines) + "\n")
    print("\n".join(lines))
    return 0


def g25(n60, n180, systems, rates):
    checks = [("band at 60 s >= %d" % G25_BAND[60], n60 >= G25_BAND[60]),
              ("band at 180 s >= %d" % G25_BAND[180], n180 >= G25_BAND[180]),
              ("axiom systems >= %d" % G25_SYSTEMS, systems >= G25_SYSTEMS)]
    for w in WALLS:
        checks.append(("A/A disagreement at %d s <= %g" % (w, G25_AA),
                       rates.get(w) is not None and rates[w] <= G25_AA))
    return checks, all(ok for _t, ok in checks)


def cmd_gate(a):
    devs = []
    with open(os.path.join(a.band, "devs.tsv"), encoding="utf-8") as fp:
        for line in fp:
            if line.strip() and not line.startswith("#"):
                devs.append(dict(zip(DEVS_COLUMNS, line.rstrip("\n").split("\t"))))
    cond = dict(ln.rstrip("\n").split("=", 1) for ln in
                open(os.path.join(a.band, "conditions.txt"), encoding="utf-8") if "=" in ln)
    rows = read_rows(a.rows)
    problems, sha, rcond = validity(rows)
    if rows and (sha != cond.get("bin_sha") or rcond != cond.get("cond")):
        problems.append(("invalid", "the A/A rows ran on another binary or under other conditions "
                         "than the measurement"))
    idx, dups = index_rows(rows)
    names = [d["name"] for d in devs]
    missing = [(n, w, p) for n in names for w in WALLS for p in ("1", "2")
               if (n, wall_key(w), p) not in idx]
    if missing:
        problems.append(("invalid", "%d A/A runs have no row (first: %s at %d s pass %s)"
                         % ((len(missing),) + missing[0])))
    if any(k[0] in set(names) for k in dups):
        problems.append(("invalid", "A/A runs with two rows"))
    rc = report_validity(problems)
    if rc:
        return rc
    n = len(devs)
    rates, lines = {}, []
    for w in WALLS:
        d = sum(1 for x in names if is_solution(idx[(x, wall_key(w), "1")])
                != is_solution(idx[(x, wall_key(w), "2")]))
        rates[w] = d / n if n else None
        lines.append("A/A at %d s: D = %d of %d DEV-S problems%s" % (
            w, d, n, "" if not n else " (%.2f%%)" % (100.0 * rates[w])))
    n60 = sum(int(d["band60"]) for d in devs)
    n180 = sum(int(d["band180"]) for d in devs)
    systems = len({d["axkey"] for d in devs})
    checks, ok = g25(n60, n180, systems, rates)
    lines.append("band at 60 s %d, band at 180 s %d, axiom systems %d" % (n60, n180, systems))
    lines += ["%s: %s" % (t, "PASS" if c else "FAIL") for t, c in checks]
    lines.append("G2.5: %s" % ("PASS" if ok else "FAIL"))
    print("\n".join(lines))
    return 0 if ok else 1


# ----------------------------------------------------------------------------


def build_parser():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0],
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd")
    p = sub.add_parser("lanes", help="the source lanes of a lane table")
    p.add_argument("lane_table", help="FindProof --list-lanes=flags output (name TAB flags)")
    p = sub.add_parser("jobs", help="the source list and the job slots")
    p.add_argument("--problems", required=True, help="the matrix problems.tsv")
    p.add_argument("--dev-t", required=True)
    p.add_argument("--train-t", required=True)
    p.add_argument("--lane-table", required=True)
    p.add_argument("--slots", type=int, default=8)
    p.add_argument("--out", required=True)
    p = sub.add_parser("extract", help="one source run's export -> candidate rows (pod)")
    p.add_argument("--root", required=True, help="the TPTP root of the source path")
    p.add_argument("--source", required=True)
    p.add_argument("--path", required=True, help="the source problem, relative to --root")
    p.add_argument("--lane", required=True)
    p.add_argument("--hx", required=True, help="the run's history export (missing: no session)")
    p.add_argument("--out", required=True, help="the row file, <source>__<lane>.cand.tsv")
    p.add_argument("--top", type=int, default=TOP)
    p.add_argument("--chunk", type=int, default=CHUNK, help=argparse.SUPPRESS)
    p = sub.add_parser("filter", help="select the candidates and write DEV-S's problems (Mac)")
    p.add_argument("--cand", action="append", required=True, help="extract files or directories")
    p.add_argument("--jobs", required=True, help="source_jobs.tsv")
    p.add_argument("--pool-root", required=True, help="the extracted tptp_pool.tgz")
    p.add_argument("--pool-problems", required=True, help="the matrix problems.tsv")
    p.add_argument("--corpus-root", required=True, help="the synthetic -10 corpus (TRAIN-X, DEV-X)")
    p.add_argument("--j13-dir", required=True, help="the J13 UEQ problem files (goals only)")
    p.add_argument("--per-system", type=int, default=PER_SYSTEM)
    p.add_argument("--cap", type=int, default=CAP)
    p.add_argument("--out", required=True)
    p = sub.add_parser("stage", help="the misses of one stage at one wall")
    p.add_argument("--problems", required=True)
    p.add_argument("--rows", action="append", required=True, help="rows.tsv or run_arms out dirs")
    p.add_argument("--wall", type=int, required=True)
    p.add_argument("--out", required=True)
    p = sub.add_parser("band", help="band flags and the DEV-S list")
    p.add_argument("--manifest", required=True, help="filter's candidates.tsv")
    p.add_argument("--rows", action="append", required=True)
    p.add_argument("--out", required=True)
    p = sub.add_parser("gate", help="G2.5")
    p.add_argument("--band", required=True, help="band's output directory")
    p.add_argument("--rows", action="append", required=True, help="the A/A rows")
    return ap


def main(argv=None):
    try:
        a = build_parser().parse_args(argv)
        cmds = {"lanes": cmd_lanes, "jobs": cmd_jobs, "extract": cmd_extract,
                "filter": cmd_filter, "stage": cmd_stage, "band": cmd_band, "gate": cmd_gate}
        if a.cmd not in cmds:
            build_parser().print_help()
            return 2
        return cmds[a.cmd](a)
    except SystemExit as e:
        # run_deep's thread drops a SystemExit silently: print its message here.
        if isinstance(e.code, str):
            print(e.code, file=sys.stderr)
            return 2
        return e.code if isinstance(e.code, int) else 2


if __name__ == "__main__":
    sys.exit(D.run_deep(main))
