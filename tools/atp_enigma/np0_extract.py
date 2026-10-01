#!/usr/bin/env python3
# Turn one problem's history export (THVM_ATP_HISTORY_EXPORT,
# src/atp/history.c, format version 1 or 2) plus the TSTP proof the same
# run emitted (--proof) into a compact labelled dataset.
#
# Labels: positives are the trace-DAG ancestors of every trace record the
# emitted proof names or cites (cnf(t<N>...) records and their inference
# parent lists).  The closure runs over the exported DAG: CP entries ->
# (outer, inner) parent rules, rule/simplify entries -> child 0 (source,
# NORM_STEP chains included) and child 1 (NORM_STEP: the rewriting rule).
# thvm_atp_cp_label is NOT used (single-normal-form extractor: labels
# nothing on multi-normal-form closures).
#
# Output (.npz): every queued CP the export saw ('P' records; the export
# rate and a further uniform seq-hash subsample are recorded), every
# selected CP ('S'), the rule-side DAG ('R'), per-selection seq
# boundaries, parent-death events, and -- when the run armed
# THVM_ATP_HISTORY_CPFEAT -- each pushed CP's 14 engine features ('F':
# thvm_atp_cp_features at push, cpfeat / has_cpfeat).  Version 2 adds the
# lane, the selection dimension of every pick (pick; 255 = not recorded,
# as in version 1), every rule's content ('Q' + 'q') with its canonical
# equation key (q_key / q_text), rule deaths ('Z'), the Otter replay
# ('O'), the key of the rule each selected CP became (rule_key) and the
# keys of the rules in the proof's derivation DAG (proof_keys) -- the
# cross-lane label source.  See extract() for the arrays.
#
# The canonical equation key renders both orientations of an equation with
# symbol names from the 'Y' records (c<label> where a name is missing) and
# variables renamed X0, X1, ... by first occurrence, and keeps the smaller
# text; q_key is its 64-bit BLAKE2b digest.  Keys compare across lanes
# that share the same input transform (the plan's cross-lane rule).
#
# Runs without a proof (failed runs, or proof.out "-") are extracted in
# no-proof mode: no proof labels, but the rule records and the hindsight
# labels (hind_label: ancestors of the rules born in the run's second
# half; q_late marks those rules), the plan's section 3.2 hindsight
# positives.  sig fingerprints the input signature ('Y' records): match
# proof_keys across lanes only between equal sigs; n_unnamed counts the
# function symbols without a name (their keys cannot match).
#
# Usage: np0_extract.py <hx.bin> <proof.out|-> <out.npz> [max_neg=300000]
#        np0_extract.py --stats <hx.bin>
#        np0_extract.py --rules <hx.bin>   (TSV of every rule record)
#        np0_extract.py --selftest         (reads synthetic v1 and v2 files)
import hashlib
import os
import re
import sys
import tempfile

import numpy as np

REC = np.dtype([
    ("tag", "u1"), ("kind", "u1"), ("flags", "u1"), ("pos_len", "u1"),
    ("seq", "<u4"), ("step", "<u4"), ("trace", "<u4"), ("pa", "<u4"),
    ("pb", "<u4"), ("pri", "<u4"), ("uid_a", "<u4"), ("uid_b", "<u4"),
    ("depth", "<u4"), ("aux", "<u4"), ("feat", "<u2", (10,)),
])
assert REC.itemsize == 64
FREC = np.dtype([
    ("tag", "u1"), ("kind", "u1"), ("flags", "u1"), ("pad", "u1"),
    ("seq", "<u4"), ("f", "<f4", (14,)),
])
assert FREC.itemsize == 64
HREC = np.dtype([                     # 'B' (version 2 layout)
    ("tag", "u1"), ("kind", "u1"), ("flags", "u1"), ("lane_len", "u1"),
    ("seq", "<u4"), ("step", "<u4"), ("trace", "<u4"), ("pa", "<u4"),
    ("pb", "<u4"), ("pri", "<u4"), ("lane", "S36"),
])
assert HREC.itemsize == 64
YREC = np.dtype([                     # 'Y' symbol
    ("tag", "u1"), ("arity", "u1"), ("flags", "u1"), ("name_len", "u1"),
    ("label", "<u4"), ("name", "S56"),
])
assert YREC.itemsize == 64
CREC = np.dtype([                     # 'q' continuation
    ("tag", "u1"), ("index", "u1"), ("bytes", "u1", (62,)),
])
assert CREC.itemsize == 64
NONE = 0xFFFFFFFF
TRACE_AXIOM, TRACE_ORIENT, TRACE_CP, TRACE_SIMPLIFY = 1, 2, 3, 4
TRACE_NORM_STEP, TRACE_FVI = 5, 6
T = {c: ord(c) for c in "BPSXDKkRWEFYQqZO"}
PICK = {0: "weight", 1: "age", 2: "w2", 3: "goal", 4: "random", 5: "kdh",
        6: "topk", 255: "unknown"}
RULE_BORN, RULE_FVI, RULE_COMPOSE, RULE_PASSIVE, RULE_INSTALL = 1, 2, 3, 4, 5
RULE_KIND = {1: "born", 2: "fvi", 3: "compose", 4: "passive", 5: "install"}
Q_ORIENTED, Q_TRUNC, Q_NOCONTENT, Q_PASSIVE = 0x01, 0x02, 0x04, 0x40
OT_KIND = {1: "adopt", 2: "joined", 3: "unorient", 4: "refused", 5: "lrs",
           6: "deferred", 7: "gone", 8: "drop", 9: "retire-sel",
           10: "retire-lrs"}


def hx_hash(x):
    x = np.asarray(x, dtype=np.uint32).copy()
    x ^= x >> np.uint32(16)
    x *= np.uint32(0x7FEB352D)
    x ^= x >> np.uint32(15)
    x *= np.uint32(0x846CA68B)
    x ^= x >> np.uint32(16)
    return x


def load(path):
    return np.fromfile(path, dtype=REC)


def sessions(recs):
    """(begin, end, E record or None) for every session in the file."""
    out = []
    bs = np.nonzero(recs["tag"] == T["B"])[0]
    for i, b in enumerate(bs):
        stop = bs[i + 1] if i + 1 < len(bs) else len(recs)
        es = np.nonzero(recs["tag"][b:stop] == T["E"])[0]
        e = b + es[-1] if len(es) else None
        out.append((b, stop, e))
    return out


def header(recs, b):
    """(format version, lane) of the session starting at record b."""
    h = recs[b:b + 1].view(HREC)[0]
    version = int(h["kind"]) or 1    # version 1 left the kind byte 0
    if version < 2:
        return version, ""
    lane = bytes(h["lane"])[:int(h["lane_len"])]
    return version, lane.decode("utf-8", "replace")


def fof_branch(recs, b):
    """The fof-split branch index of the session at record b, or None.
    A fof-split proof stitches every branch's proof; one session holds
    one branch's search only."""
    h = recs[b:b + 1].view(HREC)[0]
    if int(h["kind"]) >= 2 and int(h["flags"]) & 2:
        return int(h["pb"])
    return None


def signature(names):
    """Fingerprint of the session's input signature: a hash of the sorted
    (name, arity, fresh) 'Y' records.  Cross-lane keys are comparable only
    between sessions with equal signatures (the same input transform)."""
    txt = "\n".join(f"{nm}/{ar}/{(fl >> 1) & 1}"
                    for nm, ar, fl in sorted(names.values()))
    d = hashlib.blake2b(txt.encode("utf-8"), digest_size=8).digest()
    return int.from_bytes(d, "little")


def unnamed_labels(rules, names):
    """Function symbols in the decoded rules with no 'Y' record (rendered
    c<label>, so their keys cannot match across lanes)."""
    seen = set()
    for q in rules:
        if q["lhs"] is None:
            continue
        stack = [q["lhs"], q["rhs"]]
        while stack:
            t = stack.pop()
            if t[0] == "v":
                continue
            if t[0] not in names:
                seen.add(t[0])
            stack.extend(t[1])
    return seen


def r_steps(R, X, start_step):
    """Creation step of every 'R' entry.  Version 2 derives it from the
    'X' records: pa = trace size at the pick, step = the pick's step, so an
    entry's step is the step of the last pick whose pa <= its id (the
    session's start step before the first pick).  Older files stored it."""
    st = R["step"].astype(np.int64)
    if len(R) == 0 or not np.all(R["step"] == NONE):
        return st
    xs = X[X["pa"] != NONE]
    if len(xs) == 0:
        return np.full(len(R), start_step, dtype=np.int64)
    ntr = xs["pa"].astype(np.int64)
    j = np.searchsorted(ntr, R["seq"].astype(np.int64), side="right") - 1
    xstep = xs["step"].astype(np.int64)
    return np.where(j >= 0, xstep[np.maximum(j, 0)], start_step)


def symbols(recs, b, stop):
    """label -> (name, arity, flags) from the session's 'Y' records.  A
    name cut at 56 bytes (flags bit 0) gets "#<label>" appended, so two
    long names that share their first 56 bytes stay apart."""
    r = recs[b:stop]
    y = r.view(YREC)[r["tag"] == T["Y"]]
    out = {}
    for rec in y:
        label = int(rec["label"])
        flags = int(rec["flags"])
        name = bytes(rec["name"])[:int(rec["name_len"])].decode(
            "utf-8", "replace")
        if flags & 1:
            name += f"#{label}"
        out[label] = (name, int(rec["arity"]), flags)
    return out


def _varint(buf, at):
    v, shift = 0, 0
    while True:
        if at >= len(buf):
            raise ValueError("truncated varint")
        c = buf[at]
        at += 1
        v |= (c & 0x7F) << shift
        if not c & 0x80:
            return v, at
        shift += 7


def decode_term(buf, at=0):
    """One preorder side of a 'Q' encoding: ('v', id) or (label, kids).

    Iterative: a rule's depth is unbounded and Python's recursion is not."""
    stack = []                    # [label, arity, kids so far]
    while True:
        v, at = _varint(buf, at)
        if v & 1:
            node = ("v", v >> 1)
        else:
            n, at = _varint(buf, at)
            if n:
                stack.append([v >> 1, n, []])
                continue
            node = (v >> 1, ())
        while stack:
            top = stack[-1]
            top[2].append(node)
            if len(top[2]) < top[1]:
                break
            stack.pop()
            node = (top[0], tuple(top[2]))
        else:
            return node, at


def render(t, names, var):
    """Text of term t; variables become X<k> in first-occurrence order
    (var carries the numbering across both sides of an equation)."""
    out = []
    stack = [t]
    while stack:
        x = stack.pop()
        if isinstance(x, str):
            out.append(x)
        elif x[0] == "v":
            if x[1] not in var:
                var[x[1]] = len(var)
            out.append("X%d" % var[x[1]])
        elif not x[1]:
            out.append(names.get(x[0], ("c%d" % x[0], 0, 0))[0])
        else:
            out.append(names.get(x[0], ("c%d" % x[0], 0, 0))[0] + "(")
            stack.append(")")
            for j in range(len(x[1]) - 1, -1, -1):
                stack.append(x[1][j])
                if j:
                    stack.append(",")
    return "".join(out)


def canonical_key(lhs, rhs, names):
    """Orientation-free equation text and its 64-bit digest."""
    texts = []
    for a, b in ((lhs, rhs), (rhs, lhs)):
        var = {}
        sa = render(a, names, var)
        sb = render(b, names, var)
        texts.append(sa + "=" + sb)
    text = min(texts)
    digest = hashlib.blake2b(text.encode("utf-8"), digest_size=8).digest()
    return int.from_bytes(digest, "little"), text


def rule_records(recs, b, stop, names):
    """Every 'Q' record of the session with its decoded content.

    Returns a list of dicts: idx, kind, flags, trace, uid, step, src, depth,
    feat, lhs, rhs (None when the content is absent), key, text."""
    r = recs[b:stop]
    qi = np.nonzero(r["tag"] == T["Q"])[0]
    cont = r.view(CREC)
    out = []
    for i in qi.tolist():
        q = r[i]
        flags = int(q["flags"])
        n = int(q["pos_len"])
        lhs = rhs = None
        key, text = 0, ""
        if not flags & (Q_TRUNC | Q_NOCONTENT):
            span = cont[i + 1:i + 1 + n]
            ok = (len(span) == n and np.all(span["tag"] == T["q"]) and
                  np.array_equal(span["index"], np.arange(n)))
            if ok:
                ln = int(q["pa"]) + int(q["pb"])
                buf = bytes(span["bytes"].reshape(-1)[:ln])
                try:
                    lhs, at = decode_term(buf, 0)
                    rhs, at2 = decode_term(buf, at)
                    if at != int(q["pa"]) or at2 != ln:
                        lhs = rhs = None
                except ValueError:
                    lhs = rhs = None
            if lhs is not None:
                key, text = canonical_key(lhs, rhs, names)
        out.append(dict(idx=b + i, kind=int(q["kind"]), flags=flags,
                        trace=int(q["seq"]), uid=int(q["trace"]),
                        step=int(q["step"]), src=int(q["pri"]),
                        depth=int(q["depth"]), feat=q["feat"].copy(),
                        lhs=lhs, rhs=rhs, key=key, text=text))
    return out


def cited_trace_ids(proof_text):
    ids = set()
    for m in re.finditer(r"cnf\(\s*t(\d+)[A-Za-z0-9_]*\s*,", proof_text):
        ids.add(int(m.group(1)))
    for m in re.finditer(r"\[status\(thm\)\]\s*,\s*\[([^\]]*)\]", proof_text):
        for tok in m.group(1).split(","):
            tok = tok.strip()
            mm = re.match(r"t(\d+)", tok)
            if mm:
                ids.add(int(mm.group(1)))
    return ids


def proof_block(text):
    a = text.find("SZS output start")
    b = text.find("SZS output end")
    if a < 0:
        return None
    return text[a:b if b > a else len(text)]


def stats(path):
    recs = load(path)
    for (b, stop, e) in sessions(recs):
        r = recs[b:stop]
        tags, cnt = np.unique(r["tag"], return_counts=True)
        desc = " ".join(f"{chr(t)}={c}" for t, c in zip(tags, cnt))
        er = recs[e] if e is not None else None
        st = int(er["kind"]) if er is not None else -1
        version, lane = header(recs, b)
        br = fof_branch(recs, b)
        line = (f"session@{b}: v{version} lane={lane or '-'} status={st} "
                f"closed={1 if er is not None else 0} "
                f"branch={'-' if br is None else br} "
                f"n_sel={int(np.sum(r['tag'] == T['S']))} "
                f"n_r={int(np.sum(r['tag'] == T['R']))} {desc}")
        if er is not None:
            line += (f" n_trace={er['seq']} sel={er['step']} "
                     f"pushed={er['trace']} flags={er['pb']} "
                     f"rules={er['pri']} live={er['uid_a']}")
        if version >= 2:
            S = r[r["tag"] == T["S"]]
            picks, pc = np.unique(S["kind"] >> 4, return_counts=True)
            line += " picks=" + ",".join(
                f"{PICK.get(int(p), p)}:{c}" for p, c in zip(picks, pc))
            names = symbols(recs, b, stop)
            rules = rule_records(recs, b, stop, names)
            bad = sum(1 for q in rules if q["lhs"] is None and
                      not q["flags"] & (Q_TRUNC | Q_NOCONTENT))
            line += (f" symbols={len(names)} rules={len(rules)} "
                     f"rule_decode_errors={bad} "
                     f"unnamed={len(unnamed_labels(rules, names))} "
                     f"sig={signature(names):016x}")
        print(line)


def rules_tsv(path):
    recs = load(path)
    print("session\tstep\tkind\ttrace\tuid\tsrc\toriented\tpassive\tkey\ttext")
    for (b, stop, _e) in sessions(recs):
        names = symbols(recs, b, stop)
        for q in rule_records(recs, b, stop, names):
            src = "-" if q["src"] == NONE else str(q["src"])
            trace = "-" if q["trace"] == NONE else str(q["trace"])
            text = q["text"] if q["lhs"] is not None else "-"
            print(f"{b}\t{q['step']}\t{RULE_KIND.get(q['kind'], q['kind'])}"
                  f"\t{trace}\t{q['uid']}\t{src}"
                  f"\t{1 if q['flags'] & Q_ORIENTED else 0}"
                  f"\t{1 if q['flags'] & Q_PASSIVE else 0}"
                  f"\t{q['key']:016x}\t{text}")


def _ancestors(par, seeds):
    anc = set()
    stack = [t for t in seeds if t in par]
    while stack:
        t = stack.pop()
        if t in anc:
            continue
        anc.add(t)
        for p in par.get(t, ()):
            if p != NONE and p not in anc and p in par:
                stack.append(p)
    return anc


def extract(hx_path, proof_path, out_path, max_neg=300000, hind_frac=0.5):
    """Proof mode: the last PROVED session and its proof (proof_path).
    No-proof mode (proof_path "-", or no PROVED session): the last closed
    session, with hindsight labels instead -- the trace-DAG ancestors of
    the rules born ('Q' born / fvi / passive) at or after hind_frac of the
    run's selections (hind_label, q_late).  fof-split branch sessions are
    skipped: their stitched proof cites br<b>_t<N> records across
    sessions, which one session's DAG cannot label."""
    recs = load(hx_path)
    closed = [s for s in sessions(recs) if s[2] is not None]
    n_branch = sum(1 for s in closed if fof_branch(recs, s[0]) is not None)
    closed = [s for s in closed if fof_branch(recs, s[0]) is None]
    if n_branch:
        print(f"skipped {n_branch} fof-split branch session(s)")
    proved = [s for s in closed if int(recs[s[2]]["kind"]) == 1]
    blk = None
    if proof_path != "-" and proved:
        text = open(proof_path, errors="replace").read()
        blk = proof_block(text)
        if blk is None:
            print("no proof block")
            return 1
        if re.search(r"cnf\(\s*br\d+_", blk):
            print("fof-split proof: not labelled")
            return 1
        b, stop, e = proved[-1]
    elif closed:
        b, stop, e = closed[-1]
    else:
        print("no closed session")
        return 1
    r = recs[b:stop]
    E = recs[e]
    thresh_c = int(recs[b]["pri"])
    version, lane = header(recs, b)
    P = r[r["tag"] == T["P"]]
    # last 'P' per seq wins (the WM intake re-stamps the queued axioms)
    _, last = np.unique(P["seq"][::-1], return_index=True)
    P = P[len(P) - 1 - last]
    S = r[r["tag"] == T["S"]].copy()
    # Version 2 packs the selection dimension into the high nibble.
    sel_pick = (S["kind"] >> 4).astype(np.uint8) if version >= 2 else \
        np.full(len(S), 255, dtype=np.uint8)
    S["kind"] &= 0x0F
    X = r[r["tag"] == T["X"]]
    D = r[r["tag"] == T["D"]]
    K = r[r["tag"] == T["K"]]
    k = r[r["tag"] == T["k"]]
    R = r[r["tag"] == T["R"]]
    W = r[r["tag"] == T["W"]]
    Z = r[r["tag"] == T["Z"]]
    O = r[r["tag"] == T["O"]]
    F = recs.view(FREC)[b:stop][r["tag"] == T["F"]]
    if len(F):   # last 'F' per seq wins, as for 'P'
        _, lastf = np.unique(F["seq"][::-1], return_index=True)
        F = F[len(F) - 1 - lastf]
    names = symbols(recs, b, stop)
    rules = rule_records(recs, b, stop, names)
    cited = cited_trace_ids(blk) if blk is not None else set()

    # --- trace-DAG closure over R (rules/simplify/norm/axioms) + CP nodes.
    par = {}
    for rid, c0, c1 in zip(R["seq"].tolist(), R["trace"].tolist(),
                           R["pa"].tolist()):
        par[rid] = (c0, c1)
    for src in (S, P):
        for tid, pa, pb in zip(src["trace"].tolist(), src["pa"].tolist(),
                               src["pb"].tolist()):
            if tid != NONE and tid not in par:
                par[tid] = (pa, pb)
    anc = _ancestors(par, cited)
    n_cited_unknown = sum(1 for t in cited if t not in par)
    # hindsight seeds: rules born late in the run
    late_from = int(hind_frac * int(E["step"]))
    q_late = np.array([q["kind"] in (RULE_BORN, RULE_FVI, RULE_PASSIVE) and
                       q["step"] >= late_from and q["trace"] != NONE
                       for q in rules], dtype=bool)
    hind_anc = _ancestors(par, [q["trace"] for q, late in zip(rules, q_late)
                                if late])

    # --- per-CP table: union of pushed (P) and selected (S) seqs.
    sel_seq = S["seq"].astype(np.int64)
    sel_step = S["step"].astype(np.int64)
    bounds = S["aux"].astype(np.int64)          # cp_seq_next at pick t
    if len(bounds) > 1 and np.any(np.diff(bounds) < 0):
        print("warning: non-monotone selection boundaries")
    seqs = np.union1d(P["seq"].astype(np.int64), sel_seq)
    n = len(seqs)
    idx_p = np.searchsorted(seqs, P["seq"].astype(np.int64))
    idx_s = np.searchsorted(seqs, sel_seq)
    def col(dt, fill):
        return np.full(n, fill, dtype=dt)
    trace = col(np.uint32, NONE); pa = col(np.uint32, NONE)
    pb = col(np.uint32, NONE); pri = col(np.uint32, 0)
    depth = col(np.uint32, 0); kind = col(np.uint8, 0)
    flags = col(np.uint8, 0); pos_len = col(np.uint8, 255)
    uid_a = col(np.uint32, 0); uid_b = col(np.uint32, 0)
    feat = np.zeros((n, 10), dtype=np.uint16)
    pri_push = col(np.uint32, 0)
    for src, ix in ((P, idx_p), (S, idx_s)):   # S overrides P (goal syms)
        trace[ix] = src["trace"]; pa[ix] = src["pa"]; pb[ix] = src["pb"]
        depth[ix] = src["depth"]; kind[ix] = src["kind"]
        pos_len[ix] = src["pos_len"]; uid_a[ix] = src["uid_a"]
        uid_b[ix] = src["uid_b"]
        ok = (src["flags"] & 0x08) != 0
        feat[ix[ok]] = src["feat"][ok]
        flags[ix] |= src["flags"]
    pri[idx_p] = P["pri"]
    pri_push[idx_p] = P["pri"]
    cpfeat = np.zeros((n, 14), dtype=np.float32)
    has_cpfeat = np.zeros(n, bool)
    if len(F):
        idx_f = np.searchsorted(seqs, F["seq"].astype(np.int64))
        okf = (idx_f < n) & (seqs[np.minimum(idx_f, n - 1)] == F["seq"])
        cpfeat[idx_f[okf]] = F["f"][okf]
        has_cpfeat[idx_f[okf]] = True
    pri[idx_s] = S["pri"]
    pick = col(np.uint8, 255)
    pick[idx_s] = sel_pick
    has_push = np.zeros(n, bool); has_push[idx_p] = True
    # push step from the selection boundaries: #{t : bounds_t <= seq}
    push_step = np.searchsorted(bounds, seqs, side="right").astype(np.int64)
    INF = np.int64(1) << 40
    end_step = np.full(n, INF, dtype=np.int64)
    selected_at = np.full(n, -1, dtype=np.int64)
    selected_at[idx_s] = sel_step
    end_step[idx_s] = sel_step
    if len(D):
        idx_d = np.searchsorted(seqs, D["seq"].astype(np.int64))
        okd = (idx_d < n) & (seqs[np.minimum(idx_d, n - 1)] == D["seq"])
        np.minimum.at(end_step, idx_d[okd], D["step"][okd].astype(np.int64))
    orphan_on = (int(E["pb"]) & 1) != 0
    uid_on = (int(E["pb"]) & 2) != 0
    orphan_step = np.full(n, INF, dtype=np.int64)
    if orphan_on:
        if uid_on and len(K):
            kd = {}
            for u, st in zip(K["seq"].tolist(), K["step"].tolist()):
                kd.setdefault(u, st)
            for arr in (uid_a, uid_b):
                v = np.array([kd.get(x, INF) if x else INF
                              for x in arr.tolist()], dtype=np.int64)
                orphan_step = np.minimum(orphan_step, v)
        if len(k):
            kd = {}
            for u, st in zip(k["seq"].tolist(), k["step"].tolist()):
                kd.setdefault(u, st)
            is_cp = kind == TRACE_CP
            for arr in (pa, pb):
                v = np.array([kd.get(x, INF) if (c and x != NONE) else INF
                              for x, c in zip(arr.tolist(), is_cp.tolist())],
                             dtype=np.int64)
                orphan_step = np.minimum(orphan_step, v)
    # a selected CP was alive at its pick: a later death mark is moot
    orphan_step = np.where(selected_at >= 0,
                           np.where(orphan_step < selected_at, orphan_step,
                                    INF), orphan_step)
    end_step = np.minimum(end_step, orphan_step)
    label = np.array([t in anc for t in trace.tolist()], dtype=bool)
    label &= selected_at >= 0
    hind_label = np.array([t in hind_anc for t in trace.tolist()], dtype=bool)
    hind_label &= selected_at >= 0

    # --- rule content (version 2): keys per rule record, the key each
    # selected CP's rule got, and the keys of the proof DAG's rules.
    q_n = len(rules)
    q_trace = np.array([q["trace"] for q in rules], dtype=np.uint32)
    q_uid = np.array([q["uid"] for q in rules], dtype=np.uint32)
    q_step = np.array([q["step"] for q in rules], dtype=np.uint32)
    q_kind = np.array([q["kind"] for q in rules], dtype=np.uint8)
    q_flags = np.array([q["flags"] for q in rules], dtype=np.uint8)
    q_src = np.array([q["src"] for q in rules], dtype=np.uint32)
    q_depth = np.array([q["depth"] for q in rules], dtype=np.uint32)
    q_feat = (np.stack([q["feat"] for q in rules]) if q_n
              else np.zeros((0, 10), dtype=np.uint16))
    q_key = np.array([q["key"] for q in rules], dtype=np.uint64)
    # One newline-joined string: a fixed-width array would size every row
    # by the longest rule.  Read back the first len(q_key) items of
    # str(npz["q_text"]).split("\n").
    q_text = np.array("\n".join(q["text"] for q in rules))
    q_ok = np.array([q["lhs"] is not None for q in rules], dtype=bool)
    rule_key = np.zeros(n, dtype=np.uint64)
    for q in rules:
        if q["kind"] != RULE_BORN or q["lhs"] is None or q["src"] == NONE:
            continue
        j = np.searchsorted(seqs, q["src"])
        if j < n and seqs[j] == q["src"] and rule_key[j] == 0:
            rule_key[j] = q["key"]
    proof_keys = np.unique(np.array(
        [q["key"] for q in rules if q["lhs"] is not None and q["trace"] in anc],
        dtype=np.uint64))

    # --- kept set: positives + selected + uniform seq-hash sample of the
    # pushed CPs at rate_p <= the export rate (all of them when small).
    n_push_rec = int(has_push.sum())
    rate_c = thresh_c / 4294967296.0 if thresh_c < NONE else 1.0
    rate_p = rate_c
    thresh_p = thresh_c
    if n_push_rec > max_neg:
        rate_p = rate_c * max_neg / n_push_rec
        thresh_p = int(rate_p * 4294967296.0)
    in_sample = has_push & (hx_hash(seqs.astype(np.uint32)) < np.uint32(
        min(thresh_p, NONE)))
    if thresh_p >= NONE:
        in_sample = has_push.copy()
    keep = in_sample | (selected_at >= 0)
    start_step = int(recs[b:b + 1].view(HREC)[0]["step"])
    r_step = r_steps(R, X, start_step)
    sig = signature(names)
    n_unnamed = len(unnamed_labels(rules, names))

    np.savez_compressed(
        out_path,
        seq=seqs[keep].astype(np.uint32), trace=trace[keep], pa=pa[keep],
        pb=pb[keep], pri=pri[keep], pri_push=pri_push[keep],
        depth=depth[keep], kind=kind[keep], flags=flags[keep],
        pos_len=pos_len[keep], feat=feat[keep],
        push_step=push_step[keep], end_step=end_step[keep],
        selected_at=selected_at[keep], label=label[keep],
        hind_label=hind_label[keep], q_late=q_late,
        proved=np.array(blk is not None), status=np.array(int(E["kind"])),
        sig=np.array(sig, dtype=np.uint64), n_unnamed=np.array(n_unnamed),
        in_sample=in_sample[keep], has_push=has_push[keep],
        pick=pick[keep], rule_key=rule_key[keep],
        r_id=R["seq"], r_kind=R["kind"], r_c0=R["trace"], r_c1=R["pa"],
        r_step=r_step, r_flags=R["flags"], r_feat=R["feat"],
        bounds=bounds.astype(np.uint32), sel_pick=sel_pick,
        x_kind=X["kind"], x_seq=X["seq"], x_step=X["step"], x_live=X["aux"],
        x_ntrace_pick=X["pa"], x_ntrace=X["pb"],
        w_seq=W["seq"], w_step=W["step"], w_pri=W["pri"], w_kind=W["kind"],
        w_feat=W["feat"], cpfeat=cpfeat[keep], has_cpfeat=has_cpfeat[keep],
        q_trace=q_trace, q_uid=q_uid, q_step=q_step, q_kind=q_kind,
        q_flags=q_flags, q_src=q_src, q_depth=q_depth, q_feat=q_feat,
        q_key=q_key, q_text=q_text, q_ok=q_ok, proof_keys=proof_keys,
        z_trace=Z["seq"], z_uid=Z["trace"], z_step=Z["step"],
        z_flags=Z["flags"],
        o_kind=O["kind"], o_seq=O["seq"], o_step=O["step"], o_uid=O["trace"],
        o_trace=O["pa"], o_flags=O["flags"],
        version=np.array(version), lane=np.array(lane),
        sym_label=np.array(sorted(names), dtype=np.uint32),
        sym_name=np.array([names[x][0] for x in sorted(names)],
                          dtype=np.str_),
        sym_arity=np.array([names[x][1] for x in sorted(names)],
                           dtype=np.uint8),
        meta=np.array([
            rate_p, rate_c, float(len(S)), float(E["trace"]),
            float(n_push_rec), float(len(cited)), float(len(anc)),
            float(label.sum()), float(n_cited_unknown), float(E["seq"]),
            float(orphan_on), float(uid_on),
        ]))
    print(f"ok v{version} lane={lane or '-'} "
          f"mode={'proof' if blk is not None else 'hindsight'} "
          f"sig={sig:016x} unnamed={n_unnamed} "
          f"hind_pos={int(hind_label.sum())} sel={len(S)} "
          f"pushed={int(E['trace'])} P={n_push_rec} "
          f"kept={int(keep.sum())} rate_p={rate_p:.4g} cited={len(cited)} "
          f"anc={len(anc)} pos={int(label.sum())} "
          f"cited_unknown={n_cited_unknown} R={len(R)} "
          f"F={int(has_cpfeat[keep].sum())}/{int(keep.sum())} "
          f"Q={q_n} proof_keys={len(proof_keys)} Z={len(Z)} O={len(O)}")
    return 0


# --- self-test: synthetic version-1 and version-2 files ------------------

def _st_fnv(data):
    h = 2166136261
    for b in data:
        h = ((h ^ b) * 16777619) & 0xFFFFFFFF
    return h


def _st_rec(dtype, tag, **fields):
    """One record of `dtype` (64 bytes), returned as a length-1 REC array."""
    r = np.zeros(1, dtype=dtype)
    r["tag"] = ord(tag)
    if dtype is REC:
        r["pos_len"] = 255
        r["trace"] = NONE
        r["pa"] = NONE
        r["pb"] = NONE
    for k, v in fields.items():
        r[k] = v
    return r.view(REC)


def _st_session(version, branch=None):
    """A proved session: axioms t0 and t1 queued as seqs 0 and 1; seq 0 is
    picked by weight and becomes the rule f(X, e) = X (uid 1, ORIENT t2),
    seq 1 is picked by age and joins.  Version 2 adds the lane, the symbol
    table, the rule's content, its death and an Otter adoption."""
    v2 = version >= 2
    out = []
    if v2:
        out.append(_st_rec(HREC, "B", kind=2, seq=0, trace=NONE, pa=1234,
                           pb=NONE if branch is None else branch,
                           flags=0 if branch is None else 2,
                           pri=NONE, lane=b"twee", lane_len=4))
        for label, name, arity in ((1, b"e", 0), (2, b"i", 1), (3, b"f", 2)):
            out.append(_st_rec(YREC, "Y", label=label, name=name,
                               name_len=len(name), arity=arity))
    else:
        out.append(_st_rec(REC, "B", seq=0, pri=NONE))
    for seq in (0, 1):
        out.append(_st_rec(REC, "P", kind=TRACE_AXIOM, seq=seq, trace=seq,
                           pri=10 + seq))
    out.append(_st_rec(REC, "S", kind=TRACE_AXIOM, seq=0, step=1, trace=0,
                       pri=10, aux=2))
    if v2:   # pa = trace size at the pick, pb = trace size after it
        out.append(_st_rec(REC, "X", kind=1, seq=0, step=1, aux=1, pa=2,
                           pb=3))
    else:
        out.append(_st_rec(REC, "X", kind=1, seq=0, step=1, aux=1))
    if v2:
        enc = bytes([6, 2, 1, 2, 0, 1])        # f(X0, e) | X0
        out.append(_st_rec(REC, "Q", kind=RULE_BORN, flags=Q_ORIENTED,
                           pos_len=1, seq=2, step=1, trace=1, pri=0, pa=5,
                           pb=1, aux=_st_fnv(enc)))
        c = np.zeros(1, dtype=CREC)
        c["tag"] = ord("q")
        c["bytes"][0, :len(enc)] = list(enc)
        out.append(c.view(REC))
        out.append(_st_rec(REC, "O", kind=1, seq=1, step=1, trace=2))
    out.append(_st_rec(REC, "S", kind=TRACE_AXIOM | ((1 << 4) if v2 else 0),
                       seq=1, step=2, trace=1, pri=11, aux=2))
    if v2:
        out.append(_st_rec(REC, "X", kind=2, seq=1, step=2, aux=0, pa=3,
                           pb=3))
    else:
        out.append(_st_rec(REC, "X", kind=2, seq=1, step=2, aux=0))
    if v2:
        out.append(_st_rec(REC, "Z", seq=2, step=2, trace=1))
    # Version 2 leaves the step to the reader (derived from 'X').
    st = (lambda v: NONE) if v2 else (lambda v: v)
    out.append(_st_rec(REC, "R", kind=TRACE_AXIOM, seq=0, step=st(0)))
    out.append(_st_rec(REC, "R", kind=TRACE_AXIOM, seq=1, step=st(0)))
    out.append(_st_rec(REC, "R", kind=TRACE_ORIENT, seq=2, step=st(1),
                       trace=0))
    out.append(_st_rec(REC, "E", kind=1, seq=3, step=2, trace=2, pa=2, pb=0,
                       pri=1, uid_a=0, uid_b=2, depth=0, aux=3))
    return np.concatenate(out)


_ST_PROOF = (
    "% SZS status Unsatisfiable for selftest\n"
    "% SZS output start CNFRefutation for selftest\n"
    "cnf(t0, axiom, f(X, e) = X).\n"
    "cnf(t2, plain, f(X, e) = X, inference(orient, [status(thm)], [t0])).\n"
    "% SZS output end CNFRefutation for selftest\n")


def selftest():
    """Reads synthetic version-1 and version-2 files back (pods only, like
    every test).  Prints the mismatches; returns 0 when there are none."""
    fails = []

    def check(cond, what):
        if not cond:
            fails.append(what)

    key_text = "X0=f(X0,e)"
    key = int.from_bytes(hashlib.blake2b(key_text.encode("utf-8"),
                                         digest_size=8).digest(), "little")
    names = {1: ("e", 0, 0), 3: ("f", 2, 0)}
    lhs, at = decode_term(bytes([6, 2, 1, 2, 0, 1]), 0)
    rhs, at2 = decode_term(bytes([6, 2, 1, 2, 0, 1]), at)
    check(at == 5 and at2 == 6, "decode lengths")
    check(lhs == (3, (("v", 0), (1, ()))) and rhs == ("v", 0), "decode terms")
    check(canonical_key(lhs, rhs, names) == (key, key_text), "key")
    check(canonical_key(rhs, lhs, names) == (key, key_text),
          "key ignores the orientation")
    check(canonical_key((3, (("v", 5), (1, ()))), ("v", 5), names)[0] == key,
          "key ignores variable ids")
    # i^5000(X0): deeper than Python's recursion limit.  Walked down, not
    # compared with ==, which would recurse.
    buf = bytes([4, 1] * 5000 + [1])
    got, at = decode_term(buf, 0)
    node, depth = got, 0
    while isinstance(node, tuple) and node[0] == 2 and len(node[1]) == 1:
        node, depth = node[1][0], depth + 1
    check(depth == 5000 and node == ("v", 0) and at == len(buf),
          "deep decode")
    check(len(render(got, names, {})) == len("c2(") * 5000 + 2 + 5000,
          "deep render")

    with tempfile.TemporaryDirectory() as d:
        proof = os.path.join(d, "proof.out")
        with open(proof, "w") as fp:
            fp.write(_ST_PROOF)
        for version in (1, 2):
            hx = os.path.join(d, f"v{version}.hx")
            _st_session(version).tofile(hx)
            recs = load(hx)
            ss = sessions(recs)
            check(len(ss) == 1 and ss[0][2] is not None,
                  f"v{version}: one session with its E record")
            lane = "twee" if version == 2 else ""
            check(header(recs, 0) == (version, lane), f"v{version}: header")
            npz = os.path.join(d, f"v{version}.npz")
            check(extract(hx, proof, npz) == 0, f"v{version}: extract")
            z = np.load(npz)
            check(int(z["version"]) == version, f"v{version}: version")
            check(str(z["lane"]) == lane, f"v{version}: lane")
            check(z["seq"].tolist() == [0, 1], f"v{version}: seqs")
            check(z["kind"].tolist() == [TRACE_AXIOM, TRACE_AXIOM],
                  f"v{version}: kinds without the pick")
            check(z["selected_at"].tolist() == [1, 2],
                  f"v{version}: selection steps")
            check(z["label"].tolist() == [True, False], f"v{version}: labels")
            check(z["pick"].tolist() == ([0, 1] if version == 2
                                         else [255, 255]),
                  f"v{version}: picks")
            check(z["r_step"].tolist() == [0, 0, 1], f"v{version}: R steps")
            check(bool(z["proved"]), f"v{version}: proof mode")
            if version == 2:
                check(z["q_key"].tolist() == [key], "v2: q_key")
                check(str(z["q_text"]).split("\n")[0] == key_text,
                      "v2: q_text")
                check(z["rule_key"].tolist() == [key, 0], "v2: rule_key")
                check(z["proof_keys"].tolist() == [key], "v2: proof_keys")
                check(z["z_uid"].tolist() == [1], "v2: rule death")
                check(z["o_kind"].tolist() == [1], "v2: Otter adoption")
                check(z["sym_name"].tolist() == ["e", "i", "f"],
                      "v2: symbol table")
            else:
                check(len(z["q_key"]) == 0 and len(z["proof_keys"]) == 0,
                      "v1: no rule records")
                check(z["rule_key"].tolist() == [0, 0], "v1: rule_key")
            z.close()
            stats(hx)
            rules_tsv(hx)
        # No-proof mode: the rule born at step 1 of 2 is late; its
        # ancestors are t2 and t0, so seq 0 (trace 0) is a hindsight
        # positive.
        hx = os.path.join(d, "v2.hx")
        npz = os.path.join(d, "hind.npz")
        check(extract(hx, "-", npz) == 0, "hindsight: extract")
        z = np.load(npz)
        check(not bool(z["proved"]), "hindsight: no proof")
        check(z["label"].tolist() == [False, False], "hindsight: no labels")
        check(z["hind_label"].tolist() == [True, False], "hindsight: labels")
        check(z["q_late"].tolist() == [True], "hindsight: late rule")
        check(int(z["n_unnamed"]) == 0, "hindsight: every symbol named")
        z.close()
        # A fof-split branch session is skipped.
        br = os.path.join(d, "branch.hx")
        _st_session(2, branch=1).tofile(br)
        check(fof_branch(load(br), 0) == 1, "branch: header")
        check(extract(br, proof, os.path.join(d, "br.npz")) == 1,
              "branch: skipped")
        both = os.path.join(d, "both.hx")
        np.concatenate([_st_session(1), _st_session(2)]).tofile(both)
        recs = load(both)
        ss = sessions(recs)
        check([header(recs, b)[0] for b, _stop, _e in ss] == [1, 2],
              "a v1 and a v2 session in one file")
    for f in fails:
        print(f"selftest FAILED: {f}")
    if not fails:
        print("selftest ok")
    return 1 if fails else 0


def main():
    if sys.argv[1] == "--selftest":
        return selftest()
    if sys.argv[1] == "--stats":
        stats(sys.argv[2])
        return 0
    if sys.argv[1] == "--rules":
        rules_tsv(sys.argv[2])
        return 0
    max_neg = int(sys.argv[4]) if len(sys.argv) > 4 else 300000
    return extract(sys.argv[1], sys.argv[2], sys.argv[3], max_neg)


if __name__ == "__main__":
    sys.exit(main())
