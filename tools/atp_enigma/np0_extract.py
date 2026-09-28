#!/usr/bin/env python3
# Neural-selection phase 0: turn one problem's history export
# (THVM_ATP_HISTORY_EXPORT, src/atp/history.c) plus the TSTP proof the
# same run emitted (--proof) into a compact labelled dataset.
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
# boundaries, and parent-death events; see save() for the arrays.
#
# Usage: np0_extract.py <hx.bin> <proof.out> <out.npz> [max_neg=300000]
#        np0_extract.py --stats <hx.bin>
import re
import sys

import numpy as np

REC = np.dtype([
    ("tag", "u1"), ("kind", "u1"), ("flags", "u1"), ("pos_len", "u1"),
    ("seq", "<u4"), ("step", "<u4"), ("trace", "<u4"), ("pa", "<u4"),
    ("pb", "<u4"), ("pri", "<u4"), ("uid_a", "<u4"), ("uid_b", "<u4"),
    ("depth", "<u4"), ("aux", "<u4"), ("feat", "<u2", (10,)),
])
assert REC.itemsize == 64
NONE = 0xFFFFFFFF
TRACE_AXIOM, TRACE_ORIENT, TRACE_CP, TRACE_SIMPLIFY = 1, 2, 3, 4
TRACE_NORM_STEP, TRACE_FVI = 5, 6
T = {c: ord(c) for c in "BPSXDKkRE"}


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
        print(f"session@{b}: status={st} {desc}"
              + (f" n_trace={er['seq']} sel={er['step']} pushed={er['trace']}"
                 f" flags={er['pb']} rules={er['pri']} live={er['uid_a']}"
                 if er is not None else ""))


def extract(hx_path, proof_path, out_path, max_neg=300000):
    recs = load(hx_path)
    ss = [s for s in sessions(recs) if s[2] is not None
          and int(recs[s[2]]["kind"]) == 1]
    if not ss:
        print("no PROVED session")
        return 1
    b, stop, e = ss[-1]
    r = recs[b:stop]
    E = recs[e]
    thresh_c = int(recs[b]["pri"])
    P = r[r["tag"] == T["P"]]
    S = r[r["tag"] == T["S"]]
    X = r[r["tag"] == T["X"]]
    D = r[r["tag"] == T["D"]]
    K = r[r["tag"] == T["K"]]
    k = r[r["tag"] == T["k"]]
    R = r[r["tag"] == T["R"]]
    text = open(proof_path, errors="replace").read()
    blk = proof_block(text)
    if blk is None:
        print("no proof block")
        return 1
    cited = cited_trace_ids(blk)

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
    anc = set()
    stack = [t for t in cited if t in par]
    n_cited_unknown = sum(1 for t in cited if t not in par)
    while stack:
        t = stack.pop()
        if t in anc:
            continue
        anc.add(t)
        for p in par.get(t, ()):
            if p != NONE and p not in anc and p in par:
                stack.append(p)

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
    pri[idx_s] = S["pri"]
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

    np.savez_compressed(
        out_path,
        seq=seqs[keep].astype(np.uint32), trace=trace[keep], pa=pa[keep],
        pb=pb[keep], pri=pri[keep], pri_push=pri_push[keep],
        depth=depth[keep], kind=kind[keep], flags=flags[keep],
        pos_len=pos_len[keep], feat=feat[keep],
        push_step=push_step[keep], end_step=end_step[keep],
        selected_at=selected_at[keep], label=label[keep],
        in_sample=in_sample[keep], has_push=has_push[keep],
        r_id=R["seq"], r_kind=R["kind"], r_c0=R["trace"], r_c1=R["pa"],
        r_step=R["step"], r_flags=R["flags"], r_feat=R["feat"],
        bounds=bounds.astype(np.uint32),
        x_kind=X["kind"], x_seq=X["seq"], x_step=X["step"],
        meta=np.array([
            rate_p, rate_c, float(len(S)), float(E["trace"]),
            float(n_push_rec), float(len(cited)), float(len(anc)),
            float(label.sum()), float(n_cited_unknown), float(E["seq"]),
            float(orphan_on), float(uid_on),
        ]))
    print(f"ok sel={len(S)} pushed={int(E['trace'])} P={n_push_rec} "
          f"kept={int(keep.sum())} rate_p={rate_p:.4g} cited={len(cited)} "
          f"anc={len(anc)} pos={int(label.sum())} "
          f"cited_unknown={n_cited_unknown} R={len(R)}")
    return 0


def main():
    if sys.argv[1] == "--stats":
        stats(sys.argv[2])
        return 0
    max_neg = int(sys.argv[4]) if len(sys.argv) > 4 else 300000
    return extract(sys.argv[1], sys.argv[2], sys.argv[3], max_neg)


if __name__ == "__main__":
    sys.exit(main())
