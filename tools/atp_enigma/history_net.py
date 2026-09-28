#!/usr/bin/env python3
# Neural clause selection, phase 0: a derivation-history network scoring
# critical pairs (CPs), trained offline on np0_extract.py datasets, plus
# the pre-registered offline gate (~/.cache/thvm/casc/J13/neural_p0/
# gate_spec.txt).  Pure numpy, CPU.
#
# Model ([Sud25] minus the input GNN; see docs/plans/atp_neural_selection.md):
#   axiom a        E = act(W_ax [role(a) ; phi_ax(a)] + b_ax)
#   CP c = (p, q)  E = act(W_cp [E(p) ; E(q)] + b_cp)        p outer, q inner
#   rule o from src with simplifiers S (the NORM_STEP chain, collapsed):
#                  E = act(W_or [E(src) ; mean E(S) ; kind(o)] + b_or)
#                  kind: oriented from a CP / axiom / requeued equation,
#                  free-variable instance, RHS-composed rule
#   requeued simplification s of old rule r by S:
#                  E = act(W_sq [E(r) ; mean E(S)] + b_sq)
#   score(c) = w2 . relu(W1 [E(c) ; phi(c)] + b1) + b2   (logit, fixed at push)
# act = tanh (bounded: derivation recursion runs hundreds of levels deep).
# phi(c): the export's structural features, the engine priority at push,
# formation depth, overlap depth, origin kind and flags, standardized.
#
# Usage:
#   history_net.py train  <npz_dir> <pool.tsv> <out.npz> [dim=32 hid=32 epochs=40 seed=0]
#   history_net.py gate   <model.npz> <npz_dir> <pool.tsv> <out_prefix>
#   history_net.py summary <harvest.tsv> <npz_dir> <out.txt>
#   history_net.py gradcheck
import hashlib
import os
import sys
import time

import numpy as np

NONE = 0xFFFFFFFF
TRACE_AXIOM, TRACE_ORIENT, TRACE_CP, TRACE_SIMPLIFY = 1, 2, 3, 4
TRACE_NORM_STEP, TRACE_FVI = 5, 6
NT_AXIOM, NT_CP, NT_RULE, NT_SIMPQ = 0, 1, 2, 3
N_RULE_KINDS = 5     # rule from: CP, axiom, requeued eq, FVI, RHS compose
N_PHI = 28
N_PHI_AX = 20


# --------------------------------------------------------------------------
# problem graphs

def phi_struct(feat, flags):
    """18 structural columns from the export's 10 u16 features."""
    f = feat.astype(np.float64)
    n = f.shape[0]
    out = np.zeros((n, 18))
    out[:, 0] = np.log1p(f[:, 0])
    out[:, 1] = np.log1p(f[:, 1])
    out[:, 2] = f[:, 2]
    out[:, 3] = f[:, 3]
    out[:, 4] = f[:, 4]
    out[:, 5] = np.log1p(f[:, 5])
    out[:, 6] = f[:, 6]
    out[:, 7] = np.log1p(f[:, 7])
    fn_nodes = np.maximum(1.0, f[:, 0] + f[:, 1] - f[:, 5])
    out[:, 8] = f[:, 7] / fn_nodes
    out[:, 9] = f[:, 8]
    shape = feat[:, 9].astype(np.int64)
    for b in range(8):
        out[:, 10 + b] = (shape >> b) & 1
    return out


def phi_cp(d):
    feat = d["feat"]
    flags = d["flags"].astype(np.int64)
    n = len(feat)
    out = np.zeros((n, N_PHI))
    out[:, :18] = phi_struct(feat, flags)
    pri = np.where(d["has_push"], d["pri_push"], d["pri"]).astype(np.float64)
    out[:, 18] = np.log1p(pri)
    out[:, 19] = np.log1p(d["depth"].astype(np.float64))
    pl = d["pos_len"].astype(np.float64)
    out[:, 20] = np.where(pl >= 255, 0.0, pl)
    kind = d["kind"].astype(np.int64)
    out[:, 21] = kind == TRACE_AXIOM
    out[:, 22] = kind == TRACE_CP
    out[:, 23] = kind == TRACE_SIMPLIFY
    out[:, 24] = (flags >> 1) & 1       # raw (untreated) class
    out[:, 25] = (flags >> 4) & 1       # goal-derived
    out[:, 26] = flags & 1              # ultimate
    out[:, 27] = (flags >> 5) & 1       # goal symbol set known
    return out


class Problem:
    """One problem's history DAG + scored CPs, preprocessed for batching."""

    def __init__(self, path):
        d = dict(np.load(path))
        self.name = os.path.basename(path)[:-4]
        self.meta = d["meta"]
        self.rate = float(d["meta"][0])
        n_cp = len(d["seq"])
        # --- node table keyed by trace id
        tid_node = {}
        ntype, in1, in2, rkind = [], [], [], []
        simp = []          # per node: list of simplifier trace ids
        ax_phi_rows = []   # (node, phi_ax)
        r_id = d["r_id"].astype(np.int64)
        r_kind = d["r_kind"].astype(np.int64)
        r_c0 = d["r_c0"].astype(np.int64)
        r_c1 = d["r_c1"].astype(np.int64)
        rk = dict(zip(r_id.tolist(), r_kind.tolist()))
        rc0 = dict(zip(r_id.tolist(), r_c0.tolist()))
        rc1 = dict(zip(r_id.tolist(), r_c1.tolist()))
        # queued-axiom goal-derived bit (role)
        kind_cp = d["kind"].astype(np.int64)
        gd_of = {}
        for t, k, fl in zip(d["trace"].tolist(), kind_cp.tolist(),
                            d["flags"].tolist()):
            if k == TRACE_AXIOM:
                gd_of[t] = (fl >> 4) & 1
        r_phi = phi_struct(d["r_feat"], d["r_flags"])
        r_featok = (d["r_flags"].astype(np.int64) & 0x08) != 0
        # CP entries (trace id -> parents) from the CP table
        cp_par = {}
        for t, pa, pb, k in zip(d["trace"].tolist(), d["pa"].tolist(),
                                d["pb"].tolist(), kind_cp.tolist()):
            if k == TRACE_CP and t != NONE:
                cp_par[t] = (pa, pb)
        events = []   # (trace id, node type, payload)
        for i, (t, k) in enumerate(zip(r_id.tolist(), r_kind.tolist())):
            if k == TRACE_NORM_STEP:
                continue
            events.append((t, k, i))
        for t in cp_par:
            events.append((t, TRACE_CP, None))
        events.sort(key=lambda e: e[0])

        def chain(t):
            """Collapse a NORM_STEP chain: (source trace id, simplifiers)."""
            ss = []
            guard = 0
            while t != NONE and rk.get(t) == TRACE_NORM_STEP and guard < 100000:
                c1 = rc1[t]
                if c1 != NONE:
                    ss.append(c1)
                t = rc0[t]
                guard += 1
            return t, ss

        for t, k, i in events:
            node = len(ntype)
            if k == TRACE_AXIOM:
                ntype.append(NT_AXIOM); in1.append(-1); in2.append(-1)
                rkind.append(0); simp.append([])
                ph = np.zeros(N_PHI_AX)
                if r_featok[i]:
                    ph[:18] = r_phi[i]
                ph[18] = 1.0 - gd_of.get(t, 0)
                ph[19] = gd_of.get(t, 0)
                ax_phi_rows.append((node, ph))
            elif k == TRACE_CP:
                pa, pb = cp_par[t]
                ntype.append(NT_CP)
                in1.append(tid_node.get(pa, -1)); in2.append(tid_node.get(pb, -1))
                rkind.append(0); simp.append([])
            elif k in (TRACE_ORIENT, TRACE_FVI):
                src, ss = chain(rc0[t])
                sk = rk.get(src, TRACE_CP if src in cp_par else -1)
                if k == TRACE_FVI:
                    kk = 3
                elif sk == TRACE_AXIOM:
                    kk = 1
                elif sk == TRACE_SIMPLIFY:
                    kk = 2
                elif sk in (TRACE_ORIENT, TRACE_FVI):
                    kk = 4
                else:
                    kk = 0
                ntype.append(NT_RULE)
                in1.append(tid_node.get(src, -1)); in2.append(-1)
                rkind.append(kk)
                simp.append([tid_node[x] for x in ss if x in tid_node])
            elif k == TRACE_SIMPLIFY:
                src, ss = chain(rc0[t])
                ntype.append(NT_SIMPQ)
                in1.append(tid_node.get(src, -1)); in2.append(-1)
                rkind.append(0)
                simp.append([tid_node[x] for x in ss if x in tid_node])
            else:
                continue
            tid_node[t] = node
        nn = len(ntype)
        self.ntype = np.array(ntype, dtype=np.int64)
        self.in1 = np.array(in1, dtype=np.int64)
        self.in2 = np.array(in2, dtype=np.int64)
        self.rkind = np.array(rkind, dtype=np.int64)
        # simplifier membership as (owner, member) pairs
        so, sm = [], []
        for node, lst in enumerate(simp):
            for m in lst:
                so.append(node); sm.append(m)
        self.s_owner = np.array(so, dtype=np.int64)
        self.s_member = np.array(sm, dtype=np.int64)
        cnt = np.bincount(self.s_owner, minlength=nn) if len(so) else np.zeros(nn)
        self.s_cnt = np.maximum(cnt, 1).astype(np.float64)
        self.ax_phi = np.zeros((nn, N_PHI_AX))
        for node, ph in ax_phi_rows:
            self.ax_phi[node] = ph
        # longest-path levels (node order = trace-id order = topological)
        self.level = np.zeros(nn, dtype=np.int64)
        fix_levels(self)
        # --- scored CPs -> nodes
        trace = d["trace"].astype(np.int64)
        node_of = np.full(n_cp, -1, dtype=np.int64)
        for i, t in enumerate(trace.tolist()):
            node_of[i] = tid_node.get(t, -1)
        self.cp_node = node_of
        # core nodes (non-CP, or an input of another node) first, then the
        # leaf CPs (never selected into a rule): a training batch carries
        # the core DAG plus only the leaves it scores.  Leaves depend only
        # on core nodes, so the permutation keeps the order topological.
        used = np.zeros(nn, dtype=bool)
        for arr in (self.in1, self.in2, self.s_member):
            used[arr[arr >= 0]] = True
        core = used | (self.ntype != NT_CP)
        perm = np.concatenate([np.nonzero(core)[0], np.nonzero(~core)[0]])
        inv = np.empty(nn, dtype=np.int64)
        inv[perm] = np.arange(nn)
        def remap(a):
            return np.where(a >= 0, inv[np.maximum(a, 0)], -1)
        self.ntype = self.ntype[perm]
        self.in1 = remap(self.in1)[perm]
        self.in2 = remap(self.in2)[perm]
        self.rkind = self.rkind[perm]
        self.level = self.level[perm]
        self.ax_phi = self.ax_phi[perm]
        self.s_cnt = self.s_cnt[perm]
        self.s_owner = remap(self.s_owner)
        self.s_member = remap(self.s_member)
        self.cp_node = remap(self.cp_node)
        self.n_core = int(core.sum())
        self.phi = phi_cp(d)
        self.label = d["label"].astype(bool)
        self.in_sample = d["in_sample"].astype(bool)
        self.selected_at = d["selected_at"].astype(np.int64)
        self.push_step = d["push_step"].astype(np.int64)
        self.end_step = d["end_step"].astype(np.int64)
        self.seq = d["seq"].astype(np.int64)
        self.pri = np.where(d["has_push"], d["pri_push"], d["pri"]).astype(np.int64)
        self.weight = d["feat"][:, 0].astype(np.int64) + d["feat"][:, 1].astype(np.int64)
        self.kind = kind_cp
        self.w_seq = d["w_seq"].astype(np.int64)
        self.w_step = d["w_step"].astype(np.int64)
        self.w_pri = d["w_pri"].astype(np.int64)
        self.n_sel = int(d["meta"][2])


def validate_levels(p):
    """Every input sits on a strictly lower level."""
    ok = True
    for arr in (p.in1, p.in2):
        m = arr >= 0
        ok &= bool(np.all(p.level[arr[m]] < p.level[np.nonzero(m)[0]]))
    if len(p.s_owner):
        ok &= bool(np.all(p.level[p.s_member] < p.level[p.s_owner]))
    return ok


def fix_levels(p):
    """Longest-path levels over in1/in2 and simplifier edges (topological
    order = node index order)."""
    nn = len(p.ntype)
    lvl = np.zeros(nn, dtype=np.int64)
    s_by_owner = {}
    for o, m in zip(p.s_owner.tolist(), p.s_member.tolist()):
        s_by_owner.setdefault(o, []).append(m)
    for v in range(nn):
        l = 0 if p.ntype[v] == NT_AXIOM else 1
        a, b = p.in1[v], p.in2[v]
        if a >= 0:
            l = max(l, lvl[a] + 1)
        if b >= 0:
            l = max(l, lvl[b] + 1)
        for m in s_by_owner.get(v, ()):
            l = max(l, lvl[m] + 1)
        lvl[v] = l
    p.level = lvl
    p.n_levels = int(lvl.max()) + 1 if nn else 1


# --------------------------------------------------------------------------
# batched forward / backward over a disjoint union of problem DAGs

class Batch:
    def __init__(self, probs, rows):
        """rows[i] = indices of problem i's scored CPs to include; the
        batch carries each problem's core DAG plus the scored leaves."""
        off = 0
        nt, a1, a2, rk, lv, axp = [], [], [], [], [], []
        so, sm, sc = [], [], []
        cn, ph, lab, grp = [], [], [], []
        for gi, (p, rr) in enumerate(zip(probs, rows)):
            C = p.n_core
            nodes = p.cp_node[rr]
            leaves = np.unique(nodes[nodes >= C])
            take = np.concatenate([np.arange(C), leaves])
            nt.append(p.ntype[take])
            a1.append(np.where(p.in1[take] >= 0, p.in1[take] + off, -1))
            a2.append(np.where(p.in2[take] >= 0, p.in2[take] + off, -1))
            rk.append(p.rkind[take]); lv.append(p.level[take])
            axp.append(p.ax_phi[take])
            so.append(p.s_owner + off); sm.append(p.s_member + off)
            sc.append(p.s_cnt[take])
            loc = np.where(nodes >= C, C + np.searchsorted(leaves, nodes),
                           nodes)
            cn.append(np.where(nodes >= 0, loc + off, -1))
            ph.append(p.phi[rr]); lab.append(p.label[rr])
            grp.append(np.full(len(rr), gi))
            off += len(take)
        self.N = off
        self.ntype = np.concatenate(nt) if nt else np.zeros(0, np.int64)
        self.in1 = np.concatenate(a1); self.in2 = np.concatenate(a2)
        self.rkind = np.concatenate(rk); self.level = np.concatenate(lv)
        self.ax_phi = np.concatenate(axp)
        self.s_owner = np.concatenate(so); self.s_member = np.concatenate(sm)
        self.s_cnt = np.concatenate(sc)
        self.cp_node = np.concatenate(cn); self.phi = np.concatenate(ph)
        self.label = np.concatenate(lab); self.group = np.concatenate(grp)
        self.n_groups = len(probs)
        L = int(self.level.max()) + 1 if self.N else 1
        order = np.argsort(self.level, kind="stable")
        bounds = np.searchsorted(self.level[order], np.arange(L + 1))
        self.lv_nodes = []
        for l in range(L):
            vs = order[bounds[l]:bounds[l + 1]]
            per = {}
            for t in (NT_AXIOM, NT_CP, NT_RULE, NT_SIMPQ):
                sel = vs[self.ntype[vs] == t]
                if len(sel):
                    per[t] = sel
            self.lv_nodes.append(per)
        # simplifier edges grouped by owner level
        if len(self.s_owner):
            olv = self.level[self.s_owner]
            o2 = np.argsort(olv, kind="stable")
            b2 = np.searchsorted(olv[o2], np.arange(L + 1))
            self.lv_simp = [o2[b2[l]:b2[l + 1]] for l in range(L)]
        else:
            self.lv_simp = [np.zeros(0, np.int64) for _ in range(L)]


def init_params(dim, hid, rng):
    def lin(o, i):
        return rng.normal(0.0, 1.0 / np.sqrt(i), size=(o, i))
    return {
        "W_ax": lin(dim, N_PHI_AX), "b_ax": np.zeros(dim),
        "W_cp": lin(dim, 2 * dim), "b_cp": np.zeros(dim),
        "W_or": lin(dim, 2 * dim + N_RULE_KINDS), "b_or": np.zeros(dim),
        "W_sq": lin(dim, 2 * dim), "b_sq": np.zeros(dim),
        "miss": rng.normal(0.0, 0.1, size=dim),
        "W1": lin(hid, dim + N_PHI), "b1": np.zeros(hid),
        "w2": lin(1, hid)[0], "b2": np.zeros(1),
        "mu_phi": np.zeros(N_PHI), "sd_phi": np.ones(N_PHI),
        "mu_ax": np.zeros(N_PHI_AX), "sd_ax": np.ones(N_PHI_AX),
    }


TRAINABLE = ["W_ax", "b_ax", "W_cp", "b_cp", "W_or", "b_or", "W_sq", "b_sq",
             "miss", "W1", "b1", "w2", "b2"]


def forward(P, B, need_cache=True):
    dim = P["b_cp"].shape[0]
    H = np.zeros((B.N, dim))
    cache = []
    miss = P["miss"]

    def gather(idx):
        g = H[np.maximum(idx, 0)]
        m = idx < 0
        if m.any():
            g[m] = miss
        return g

    axn = (B.ax_phi - P["mu_ax"]) / P["sd_ax"]
    for l, per in enumerate(B.lv_nodes):
        se = B.lv_simp[l]      # simplifier edges owned by this level
        lc = {}
        for t, vs in per.items():
            if t == NT_AXIOM:
                X = axn[vs]
                Z = X @ P["W_ax"].T + P["b_ax"]
            elif t == NT_CP:
                X = np.concatenate([gather(B.in1[vs]), gather(B.in2[vs])], 1)
                Z = X @ P["W_cp"].T + P["b_cp"]
            else:
                A = np.zeros((len(vs), dim))
                if len(se):
                    pos = np.full(B.N, -1, dtype=np.int64)
                    pos[vs] = np.arange(len(vs))
                    own = B.s_owner[se]
                    keep = pos[own] >= 0
                    if keep.any():
                        np.add.at(A, pos[own[keep]], H[B.s_member[se[keep]]])
                        A /= B.s_cnt[vs][:, None]
                src = gather(B.in1[vs])
                if t == NT_RULE:
                    oh = np.zeros((len(vs), N_RULE_KINDS))
                    oh[np.arange(len(vs)), B.rkind[vs]] = 1.0
                    X = np.concatenate([src, A, oh], 1)
                    Z = X @ P["W_or"].T + P["b_or"]
                else:
                    X = np.concatenate([src, A], 1)
                    Z = X @ P["W_sq"].T + P["b_sq"]
            Hv = np.tanh(Z)
            H[vs] = Hv
            if need_cache:
                lc[t] = (vs, X, Hv)
        cache.append(lc)
    E = gather(B.cp_node)
    phin = (B.phi - P["mu_phi"]) / P["sd_phi"]
    F = np.concatenate([E, phin], 1)
    Z1 = F @ P["W1"].T + P["b1"]
    A1 = np.maximum(Z1, 0.0)
    logit = A1 @ P["w2"] + P["b2"][0]
    return logit, (H, cache, E, F, Z1, A1)


def loss_grad(P, B, logit, fw, pos_w=0.5):
    """Class-balanced logistic loss, each problem weighted equally."""
    y = B.label.astype(np.float64)
    G = B.n_groups
    npos = np.bincount(B.group, weights=y, minlength=G)
    nneg = np.bincount(B.group, weights=1 - y, minlength=G)
    wpos = np.where(npos > 0, pos_w / np.maximum(npos, 1), 0.0)
    wneg = np.where(nneg > 0, (1 - pos_w) / np.maximum(nneg, 1), 0.0)
    w = np.where(y > 0, wpos[B.group], wneg[B.group]) / G
    # stable log-sigmoid
    lp = -np.logaddexp(0.0, -logit)      # log sigma(z)
    ln = -np.logaddexp(0.0, logit)       # log (1 - sigma(z))
    loss = -np.sum(w * (y * lp + (1 - y) * ln))
    sig = 1.0 / (1.0 + np.exp(-logit))
    dlogit = w * (sig - y)
    return loss, dlogit


def backward(P, B, fw, dlogit):
    H, cache, E, F, Z1, A1 = fw
    dim = P["b_cp"].shape[0]
    g = {k: np.zeros_like(P[k]) for k in TRAINABLE}
    g["w2"] = A1.T @ dlogit
    g["b2"] = np.array([dlogit.sum()])
    dA1 = np.outer(dlogit, P["w2"])
    dZ1 = dA1 * (Z1 > 0)
    g["W1"] = dZ1.T @ F
    g["b1"] = dZ1.sum(0)
    dF = dZ1 @ P["W1"]
    dE = dF[:, :dim]
    dH = np.zeros((B.N, dim))

    def scatter(idx, grad):
        m = idx >= 0
        if m.any():
            np.add.at(dH, idx[m], grad[m])
        if (~m).any():
            g["miss"] += grad[~m].sum(0)

    scatter(B.cp_node, dE)
    for l in range(len(cache) - 1, -1, -1):
        lc = cache[l]
        for t, (vs, X, Hv) in lc.items():
            dZ = dH[vs] * (1.0 - Hv * Hv)
            if t == NT_AXIOM:
                g["W_ax"] += dZ.T @ X
                g["b_ax"] += dZ.sum(0)
                continue
            if t == NT_CP:
                g["W_cp"] += dZ.T @ X
                g["b_cp"] += dZ.sum(0)
                dX = dZ @ P["W_cp"]
                scatter(B.in1[vs], dX[:, :dim])
                scatter(B.in2[vs], dX[:, dim:2 * dim])
                continue
            if t == NT_RULE:
                g["W_or"] += dZ.T @ X
                g["b_or"] += dZ.sum(0)
                dX = dZ @ P["W_or"]
            else:
                g["W_sq"] += dZ.T @ X
                g["b_sq"] += dZ.sum(0)
                dX = dZ @ P["W_sq"]
            scatter(B.in1[vs], dX[:, :dim])
            dA = dX[:, dim:2 * dim]
            se = B.lv_simp[l]
            if len(se):
                pos = np.full(B.N, -1, dtype=np.int64)
                pos[vs] = np.arange(len(vs))
                own = B.s_owner[se]
                keep = pos[own] >= 0
                if keep.any():
                    rows = pos[own[keep]]
                    gm = dA[rows] / B.s_cnt[vs][rows][:, None]
                    np.add.at(dH, B.s_member[se[keep]], gm)
    return g


class Adam:
    def __init__(self, P, lr):
        self.lr = lr
        self.m = {k: np.zeros_like(P[k]) for k in TRAINABLE}
        self.v = {k: np.zeros_like(P[k]) for k in TRAINABLE}
        self.t = 0

    def step(self, P, g, clip=5.0):
        self.t += 1
        norm = np.sqrt(sum(float(np.sum(g[k] ** 2)) for k in TRAINABLE))
        sc = min(1.0, clip / (norm + 1e-12))
        for k in TRAINABLE:
            gk = g[k] * sc
            self.m[k] = 0.9 * self.m[k] + 0.1 * gk
            self.v[k] = 0.999 * self.v[k] + 0.001 * gk * gk
            mh = self.m[k] / (1 - 0.9 ** self.t)
            vh = self.v[k] / (1 - 0.999 ** self.t)
            P[k] -= self.lr * mh / (np.sqrt(vh) + 1e-8)
        return norm


# --------------------------------------------------------------------------
# data split

def load_pool(pool_path):
    split = {}
    for line in open(pool_path):
        f = line.rstrip("\n").split("\t")
        split[f[0]] = f[4]
    return split


def is_val(name):
    return int(hashlib.sha256((name + "|val").encode()).hexdigest()[:8], 16) % 100 < 15


def load_problems(npz_dir, names):
    probs = []
    for nm in names:
        path = os.path.join(npz_dir, nm + ".npz")
        if not os.path.exists(path):
            continue
        p = Problem(path)
        if not validate_levels(p):
            fix_levels(p)
        if p.label.sum() == 0:
            continue
        probs.append(p)
    return probs


def train_rows(p, rng, max_neg):
    pos = np.nonzero(p.label & (p.cp_node >= 0))[0]
    neg = np.nonzero((~p.label) & (p.in_sample | (p.selected_at >= 0))
                     & (p.cp_node >= 0))[0]
    if len(neg) > max_neg:
        neg = rng.choice(neg, size=max_neg, replace=False)
    return np.concatenate([pos, neg])


def fit_standardizer(P, probs):
    phis = np.concatenate([p.phi for p in probs])
    P["mu_phi"] = phis.mean(0)
    P["sd_phi"] = phis.std(0) + 1e-6
    axs = np.concatenate([p.ax_phi[p.ntype == NT_AXIOM] for p in probs])
    if len(axs):
        P["mu_ax"] = axs.mean(0)
        P["sd_ax"] = axs.std(0) + 1e-6


def eval_loss(P, probs, rng, max_neg=4096):
    tot = 0.0
    for p in probs:
        rr = train_rows(p, rng, max_neg)
        B = Batch([p], [rr])
        logit, fw = forward(P, B, need_cache=False)
        l, _ = loss_grad(P, B, logit, fw)
        tot += l
    return tot / max(1, len(probs))


def cmd_train(npz_dir, pool_path, out_path, dim=32, hid=32, epochs=40,
              seed=0, batch=8, max_neg=4096, lr=2e-3, patience=6):
    split = load_pool(pool_path)
    names = sorted(n for n, s in split.items() if s == "train")
    tr_names = [n for n in names if not is_val(n)]
    va_names = [n for n in names if is_val(n)]
    t0 = time.time()
    tr = load_problems(npz_dir, tr_names)
    va = load_problems(npz_dir, va_names)
    print(f"loaded train={len(tr)} val={len(va)} in {time.time()-t0:.0f}s; "
          f"levels max={max(p.n_levels for p in tr)} "
          f"nodes max={max(len(p.ntype) for p in tr)}", flush=True)
    rng = np.random.default_rng(seed)
    P = init_params(dim, hid, rng)
    fit_standardizer(P, tr)
    opt = Adam(P, lr)
    best = (np.inf, None, -1)
    vrng = np.random.default_rng(1234)
    for ep in range(epochs):
        order = rng.permutation(len(tr))
        tl, nb = 0.0, 0
        te = time.time()
        for bi in range(0, len(order), batch):
            ps = [tr[i] for i in order[bi:bi + batch]]
            rows = [train_rows(p, rng, max_neg) for p in ps]
            B = Batch(ps, rows)
            logit, fw = forward(P, B)
            l, dl = loss_grad(P, B, logit, fw)
            g = backward(P, B, fw, dl)
            opt.step(P, g)
            tl += l; nb += 1
        vrng = np.random.default_rng(1234)
        vl = eval_loss(P, va, vrng)
        print(f"epoch {ep} train {tl/max(1,nb):.4f} val {vl:.4f} "
              f"({time.time()-te:.0f}s)", flush=True)
        if vl < best[0]:
            best = (vl, {k: v.copy() for k, v in P.items()}, ep)
        elif ep - best[2] >= patience:
            break
    Pb = best[1]
    np.savez(out_path, **Pb, dim=dim, hid=hid, best_epoch=best[2],
             best_val=best[0])
    print(f"saved {out_path} best epoch {best[2]} val {best[0]:.4f}")


# --------------------------------------------------------------------------
# pre-registered gate (gate_spec.txt)

def score_problem(P, p):
    rr = np.arange(len(p.seq))
    B = Batch([p], [rr])
    logit, _ = forward(P, B, need_cache=False)
    return logit


def engine_pri_at(p, t):
    """cp_pri at selection t: push priority, or the last reweight < t."""
    pri = p.pri.copy()
    if len(p.w_seq):
        m = p.w_step <= t - 1
        if m.any():
            ws, wp = p.w_seq[m], p.w_pri[m]
            idx = np.searchsorted(p.seq, ws)
            ok = (idx < len(p.seq)) & (p.seq[np.minimum(idx, len(p.seq) - 1)] == ws)
            pri[idx[ok]] = wp[ok]    # later reweights overwrite (step order)
    return pri


def gate_problem(p, logit, max_steps=50):
    pos_idx = np.nonzero(p.label)[0]
    T = p.n_sel
    if len(pos_idx) == 0 or T < 2:
        return None
    # candidate steps: some positive pending (push <= t-1, selected >= t)
    ps, pe = p.push_step[pos_idx], p.selected_at[pos_idx]
    cand = []
    # sweep: pending count at t = #{push<=t-1} - #{sel<=t-1}
    ev = np.zeros(T + 2, dtype=np.int64)
    np.add.at(ev, np.clip(ps + 1, 0, T + 1), 1)
    np.add.at(ev, np.clip(pe + 1, 0, T + 1), -1)
    pending = np.cumsum(ev)
    cand = np.nonzero(pending[1:T + 1] > 0)[0] + 1
    if len(cand) == 0:
        return None
    k = min(max_steps, len(cand))
    steps = np.unique(cand[np.round(np.linspace(0, len(cand) - 1, k)).astype(int)])
    rate = p.rate
    res = {"pct": {"model": [], "engine": [], "weight": []},
           "hit": {"model": [], "engine": [], "weight": []}}
    negmask = p.in_sample & ~p.label
    for t in steps:
        alive = (p.push_step <= t - 1) & (p.end_step >= t)
        P_t = np.nonzero(alive & p.label)[0]
        N_t = np.nonzero(alive & negmask)[0]
        if len(P_t) == 0:
            continue
        size = len(P_t) + len(N_t) / rate
        pri_t = engine_pri_at(p, t)
        keys = {"model": -logit, "engine": pri_t.astype(np.float64),
                "weight": p.weight.astype(np.float64)}
        for rk, key in keys.items():
            # lexicographic (key, seq): encode as sort order
            kp = key[P_t]; sp = p.seq[P_t]
            kn = key[N_t]; sn = p.seq[N_t]
            order_n = np.lexsort((sn, kn))
            kn_s, sn_s = kn[order_n], sn[order_n]
            # count negatives strictly before each positive
            lo = np.searchsorted(kn_s, kp, side="left")
            hi = np.searchsorted(kn_s, kp, side="right")
            before_n = lo.astype(np.float64)
            for j in range(len(P_t)):
                if hi[j] > lo[j]:
                    before_n[j] += np.searchsorted(sn_s[lo[j]:hi[j]], sp[j])
            order_p = np.lexsort((sp, kp))
            rank_p = np.empty(len(P_t)); rank_p[order_p] = np.arange(len(P_t))
            est = rank_p + before_n / rate
            res["pct"][rk].extend((est / size).tolist())
            res["hit"][rk].append(float(est.min() < 10))
    return res


def bootstrap(vals_by_prob, stat, n=10000, seed=0):
    rng = np.random.default_rng(seed)
    m = len(vals_by_prob)
    out = np.empty(n)
    for i in range(n):
        idx = rng.integers(0, m, m)
        out[i] = stat(idx)
    return np.percentile(out, [2.5, 97.5]), out


def cmd_gate(model_path, npz_dir, pool_path, out_prefix):
    M = dict(np.load(model_path))
    P = {k: M[k] for k in M.files if k not in ("dim", "hid", "best_epoch", "best_val")}
    split = load_pool(pool_path)
    names = sorted(n for n, s in split.items() if s == "heldout")
    rows = []
    per = []
    for nm in names:
        path = os.path.join(npz_dir, nm + ".npz")
        if not os.path.exists(path):
            continue
        p = Problem(path)
        if not validate_levels(p):
            fix_levels(p)
        if p.label.sum() == 0:
            continue
        logit = score_problem(P, p)
        r = gate_problem(p, logit)
        if r is None:
            continue
        med = {k: float(np.median(r["pct"][k])) for k in r["pct"]}
        hit = {k: float(np.mean(r["hit"][k])) for k in r["hit"]}
        per.append((nm, med, hit, len(r["hit"]["model"]),
                    len(r["pct"]["model"]), p.rate, int(p.label.sum()),
                    p.n_sel))
        rows.append(r)
    with open(out_prefix + "_per_problem.tsv", "w") as f:
        f.write("problem\tsteps\tpairs\trate\tn_pos\tmed_model\tmed_engine\t"
                "med_weight\thit_model\thit_engine\thit_weight\n")
        for nm, med, hit, ns, npair, rate, npos, _ in per:
            f.write(f"{nm}\t{ns}\t{npair}\t{rate:.4g}\t{npos}\t"
                    f"{med['model']:.5f}\t{med['engine']:.5f}\t{med['weight']:.5f}\t"
                    f"{hit['model']:.4f}\t{hit['engine']:.4f}\t{hit['weight']:.4f}\n")
    K = ["model", "engine", "weight"]
    meds = {k: np.array([x[1][k] for x in per]) for k in K}
    hits = {k: np.array([x[2][k] for x in per]) for k in K}
    lines = [f"eligible held-out problems: {len(per)}"]
    for k in K:
        ci_m, _ = bootstrap(per, lambda idx, k=k: np.median(meds[k][idx]))
        ci_h, _ = bootstrap(per, lambda idx, k=k: np.mean(hits[k][idx]))
        allp = np.concatenate([np.array(r["pct"][k]) for r in rows])
        lines.append(f"{k:7s} median-of-problem-medians {np.median(meds[k]):.5f} "
                     f"CI [{ci_m[0]:.5f}, {ci_m[1]:.5f}]  pooled-pairs median "
                     f"{np.median(allp):.5f}  top10 hit {np.mean(hits[k]):.4f} "
                     f"CI [{ci_h[0]:.4f}, {ci_h[1]:.4f}]")
    ratio_m = np.median(meds["model"]) / max(1e-12, np.median(meds["engine"]))
    ratio_h = np.mean(hits["model"]) / max(1e-12, np.mean(hits["engine"]))
    ci_rm, _ = bootstrap(per, lambda idx: np.median(meds["model"][idx]) /
                         max(1e-12, np.median(meds["engine"][idx])))
    ci_rh, _ = bootstrap(per, lambda idx: np.mean(hits["model"][idx]) /
                         max(1e-12, np.mean(hits["engine"][idx])))
    pass_m = ratio_m <= 0.5
    pass_h = ratio_h >= 2.0
    lines.append(f"ratio median model/engine {ratio_m:.4f} CI [{ci_rm[0]:.4f}, "
                 f"{ci_rm[1]:.4f}] (need <= 0.5: {'yes' if pass_m else 'no'}; "
                 f"CI clears: {'yes' if ci_rm[1] <= 0.5 else 'no'})")
    lines.append(f"ratio top10 model/engine {ratio_h:.4f} CI [{ci_rh[0]:.4f}, "
                 f"{ci_rh[1]:.4f}] (need >= 2: {'yes' if pass_h else 'no'}; "
                 f"CI clears: {'yes' if ci_rh[0] >= 2.0 else 'no'})")
    lines.append("GATE: " + ("PASS" if (pass_m and pass_h) else "FAIL"))
    # SECONDARY (not the gate): the same statistics by problem size.
    lines.append("secondary, by selections per problem (not the gate):")
    nsel = np.array([x[7] for x in per])
    for lo, hi in ((0, 100), (100, 1000), (1000, 10 ** 9)):
        m = (nsel >= lo) & (nsel < hi)
        if not m.any():
            continue
        parts = [f"  sel [{lo},{hi if hi < 10 ** 9 else 'inf'}) n={int(m.sum())}:"]
        for k in K:
            parts.append(f"{k} med {np.median(meds[k][m]):.4f} hit "
                         f"{np.mean(hits[k][m]):.3f}")
        lines.append(" ".join(parts))
    txt = "\n".join(lines)
    print(txt)
    with open(out_prefix + "_summary.txt", "w") as f:
        f.write(txt + "\n")


# --------------------------------------------------------------------------
# harvest summary

def cmd_summary(harvest_tsv, npz_dir, out_path):
    rows = [l.rstrip("\n").split("\t") for l in open(harvest_tsv)]
    lines = []
    for sp in ("train", "heldout"):
        rr = [r for r in rows if r[1] == sp]
        solved = [r for r in rr if r[2] == "Unsatisfiable"]
        ok = [r for r in solved if r[5].startswith("ok")]
        pushed, prec, pos, sel, cited = [], [], [], [], []
        for r in ok:
            path = os.path.join(npz_dir, r[0] + ".npz")
            if not os.path.exists(path):
                continue
            m = np.load(path)["meta"]
            sel.append(m[2]); pushed.append(m[3]); prec.append(m[4])
            cited.append(m[5]); pos.append(m[7])
        pos = np.array(pos); sel = np.array(sel); pushed = np.array(pushed)
        st = {}
        for r in rr:
            st[r[2]] = st.get(r[2], 0) + 1
        lines.append(
            f"{sp}: attempted {len(rr)} solved {len(solved)} extracted {len(ok)} "
            f"with>=1 positive {int((pos > 0).sum())}; status {st}")
        if len(pos):
            lines.append(
                f"  CPs pushed total {int(pushed.sum())} (P records "
                f"{int(np.sum(prec))}); per problem median {np.median(pushed):.0f} "
                f"max {pushed.max():.0f}; selections median {np.median(sel):.0f} "
                f"max {sel.max():.0f}")
            lines.append(
                f"  positives per problem mean {pos.mean():.1f} median "
                f"{np.median(pos):.0f} max {pos.max():.0f} total {int(pos.sum())}")
    txt = "\n".join(lines)
    print(txt)
    with open(out_path, "w") as f:
        f.write(txt + "\n")


# --------------------------------------------------------------------------
# gradient check on a synthetic DAG

def cmd_gradcheck():
    rng = np.random.default_rng(0)

    class Fake:
        pass
    p = Fake()
    nn = 12
    p.ntype = np.array([0, 0, 0, 1, 2, 1, 2, 3, 1, 2, 1, 1])
    p.in1 = np.array([-1, -1, -1, 0, 3, 4, 5, 4, 6, 8, 9, 6])
    p.in2 = np.array([-1, -1, -1, 1, -1, 2, -1, -1, 4, -1, 7, -1])
    p.rkind = np.array([0, 0, 0, 0, 1, 0, 0, 0, 0, 4, 0, 0])
    p.s_owner = np.array([6, 6, 9, 7]); p.s_member = np.array([4, 2, 6, 4])
    p.s_cnt = np.maximum(np.bincount(p.s_owner, minlength=nn), 1).astype(float)
    p.ax_phi = rng.normal(size=(nn, N_PHI_AX))
    p.level = np.zeros(nn, dtype=np.int64)
    fix_levels(p)
    p.n_core = 10   # nodes 10, 11 are leaf CPs (scored, used by no node)
    p.cp_node = np.array([3, 5, 8, 10, 11, 7, -1])
    p.phi = rng.normal(size=(7, N_PHI))
    p.label = np.array([1, 0, 0, 1, 0, 0, 1], dtype=bool)
    B = Batch([p], [np.arange(7)])
    P = init_params(6, 5, rng)
    logit, fw = forward(P, B)
    l, dl = loss_grad(P, B, logit, fw)
    g = backward(P, B, fw, dl)
    worst = 0.0
    for k in TRAINABLE:
        flat = P[k].reshape(-1)
        for j in rng.choice(len(flat), size=min(6, len(flat)), replace=False):
            old = flat[j]
            flat[j] = old + 1e-6
            lp, _ = loss_grad(P, B, forward(P, B, False)[0], None)
            flat[j] = old - 1e-6
            lm, _ = loss_grad(P, B, forward(P, B, False)[0], None)
            flat[j] = old
            num = (lp - lm) / 2e-6
            ana = g[k].reshape(-1)[j]
            err = abs(num - ana) / max(1e-8, abs(num) + abs(ana))
            worst = max(worst, err)
    print(f"gradcheck worst relative error {worst:.2e} "
          f"({'OK' if worst < 1e-4 else 'FAIL'})")


def cmd_selftest(npz_path):
    """Real-data consistency: subset batches score like the full batch."""
    p = Problem(npz_path)
    print(f"{p.name}: nodes {len(p.ntype)} core {p.n_core} levels "
          f"{p.n_levels} levels-valid {validate_levels(p)} cps {len(p.seq)} "
          f"pos {int(p.label.sum())} unmapped {int((p.cp_node < 0).sum())}")
    rng = np.random.default_rng(0)
    P = init_params(16, 16, rng)
    full = score_problem(P, p)
    rr = rng.choice(len(p.seq), size=min(50, len(p.seq)), replace=False)
    B = Batch([p, p], [rr, rr[::-1]])
    lg, _ = forward(P, B, need_cache=False)
    err = max(np.max(np.abs(lg[:len(rr)] - full[rr])),
              np.max(np.abs(lg[len(rr):] - full[rr[::-1]])))
    print(f"subset-vs-full max |diff| {err:.2e} ({'OK' if err < 1e-9 else 'FAIL'})")


def main():
    cmd = sys.argv[1]
    if cmd == "train":
        a = sys.argv[2:]
        kw = {}
        if len(a) > 3: kw["dim"] = int(a[3])
        if len(a) > 4: kw["hid"] = int(a[4])
        if len(a) > 5: kw["epochs"] = int(a[5])
        if len(a) > 6: kw["seed"] = int(a[6])
        cmd_train(a[0], a[1], a[2], **kw)
    elif cmd == "gate":
        cmd_gate(*sys.argv[2:6])
    elif cmd == "gradcheck":
        cmd_gradcheck()
    elif cmd == "summary":
        cmd_summary(*sys.argv[2:5])
    elif cmd == "selftest":
        cmd_selftest(sys.argv[2])


if __name__ == "__main__":
    main()
