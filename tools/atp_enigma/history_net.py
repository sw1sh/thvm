#!/usr/bin/env python3
# Neural clause selection, phase 0: a derivation-history network scoring
# critical pairs (CPs), trained offline on np0_extract.py datasets, plus
# the pre-registered offline gate (~/.cache/thvm/casc/J13/neural_p0/
# gate_spec.txt).
#
# The model runs on the tinygrad Tensor API (the spec thvm's Python
# bridge py/thvm mirrors, so moving it onto thvm is an import change):
# nn.Linear, relu, Tensor.gather (parent lookups), Tensor.scatter_reduce
# (simplifier aggregation), log_softmax (the binary cross-entropy),
# nn.optim.Adam and TinyJit; tinygrad computes every gradient.  numpy only loads
# data, lays out the batches, rebuilds the queues and computes the gate
# metrics.
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
#   score(c) = W2 relu(W1 [E(c) ; phi(c)] + b1) + b2   (logit, fixed at push)
# act(x) = relu(x) - relu(x - 6): a relu kept bounded, since the
# derivation recursion runs up to ~200 levels deep.
# phi(c): the export's structural features, the engine priority at push,
# formation depth, overlap depth, origin kind and flags, standardized.
#
# The recursion is evaluated level by level over the derivation DAG
# (every input of a node sits on a lower level): per level, one block of
# all the level's derived nodes, one batched Linear per inference kind
# over the parents' embeddings (gathered from the table of all
# embeddings computed so far), each kind's rows selected by a mask.  Block sizes and the table
# are padded to power-of-two buckets so tinygrad reuses its compiled
# kernels across problems (padding rows point at the learned "missing"
# row and carry zero loss weight).
#
# Usage:
#   history_net.py train  <npz_dir> <pool.tsv> <out_prefix> [dim=32 hid=32
#                         epochs=60 seed=0 weighting=uniform|residence
#                         warm_steps=0 lr=3e-3]
#                  (warm_steps > 0: the scoring MLP starts from a features-
#                  only head trained that many steps, saved as <out>_phi)
#   history_net.py gate   <model_prefix> <npz_dir> <pool.tsv> <out_prefix>
#   history_net.py valgate <model_prefix> <npz_dir> <pool.tsv> <out_prefix>
#                  (the same statistics on the train-carved validation slice)
#   history_net.py sanity <npz_dir> <pool.tsv>   (Adam moves the parameters,
#                  the loss falls, subset batches score like the full batch)
#   history_net.py summary <harvest.tsv> <npz_dir> <out.txt>
#   history_net.py recon  <npz_dir> <pool.tsv> <out.txt>
import hashlib
import os
import sys
import time

import numpy as np
from tinygrad import Tensor, TinyJit, nn
from tinygrad.nn.state import get_parameters, get_state_dict, safe_load, safe_save, load_state_dict

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
        # queued-axiom goal-derived bit (role) and features: the queue
        # rows' packed-byte features stand in when the trace pair is
        # packed off-heap (the default trace)
        kind_cp = d["kind"].astype(np.int64)
        gd_of, ax_feat_of = {}, {}
        cp_phi18 = phi_struct(d["feat"], d["flags"])
        for i, (t, k, fl) in enumerate(zip(d["trace"].tolist(), kind_cp.tolist(),
                                           d["flags"].tolist())):
            if k == TRACE_AXIOM:
                gd_of[t] = (fl >> 4) & 1
                if fl & 0x08:
                    ax_feat_of[t] = cp_phi18[i]
        r_phi = phi_struct(d["r_feat"], d["r_flags"])
        r_featok = (d["r_flags"].astype(np.int64) & 0x08) != 0
        # entries pushed after the search ended (proof emission) are not
        # part of the history a search-time scorer sees
        r_search = (d["r_flags"].astype(np.int64) & 0x01) == 0
        # CP entries (trace id -> parents) from the CP table
        cp_par = {}
        for t, pa, pb, k in zip(d["trace"].tolist(), d["pa"].tolist(),
                                d["pb"].tolist(), kind_cp.tolist()):
            if k == TRACE_CP and t != NONE:
                cp_par[t] = (pa, pb)
        events = []   # (trace id, node type, payload)
        for i, (t, k) in enumerate(zip(r_id.tolist(), r_kind.tolist())):
            if k == TRACE_NORM_STEP or not r_search[i]:
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
                elif t in ax_feat_of:
                    ph[:18] = ax_feat_of[t]
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
# batch layout (numpy) + model (tinygrad)

def pow2(n):
    return 1 << max(0, int(np.ceil(np.log2(max(1, n)))))


class Std:
    """Feature standardizer fitted on the train problems."""

    def __init__(self, probs=None, arrays=None):
        if arrays is not None:
            self.mu_phi, self.sd_phi, self.mu_ax, self.sd_ax = arrays
            return
        phis = np.concatenate([p.phi for p in probs])
        self.mu_phi = phis.mean(0)
        self.sd_phi = phis.std(0) + 1e-6
        axs = np.concatenate([p.ax_phi[:p.n_core][p.ntype[:p.n_core] == NT_AXIOM]
                              for p in probs])
        self.mu_ax = axs.mean(0) if len(axs) else np.zeros(N_PHI_AX)
        self.sd_ax = (axs.std(0) + 1e-6) if len(axs) else np.ones(N_PHI_AX)


class Block:
    pass


class DagLayout:
    """The core derivation DAGs of a set of problems as one disjoint
    union, laid out level-major in power-of-two padded blocks (table row
    0 is the learned 'missing' embedding).  Every input of a block sits
    on a lower level, so a block gathers from the rows computed so far."""

    def __init__(self, probs, std, dim):
        self.dim = dim
        off = 0
        nt, a1, a2, rk, lv, axp, sc = [], [], [], [], [], [], []
        so, sm = [], []
        self.offsets = []
        for p in probs:
            C = p.n_core
            self.offsets.append(off)
            nt.append(p.ntype[:C])
            a1.append(np.where(p.in1[:C] >= 0, p.in1[:C] + off, -1))
            a2.append(np.where(p.in2[:C] >= 0, p.in2[:C] + off, -1))
            rk.append(p.rkind[:C]); lv.append(p.level[:C])
            axp.append(p.ax_phi[:C]); sc.append(p.s_cnt[:C])
            so.append(p.s_owner + off); sm.append(p.s_member + off)
            off += C
        ntype = np.concatenate(nt); in1 = np.concatenate(a1)
        in2 = np.concatenate(a2); rkind = np.concatenate(rk)
        level = np.concatenate(lv); ax_phi = np.concatenate(axp)
        s_cnt = np.concatenate(sc)
        s_owner = np.concatenate(so); s_member = np.concatenate(sm)
        N = len(ntype)
        axn = (ax_phi - std.mu_ax) / std.sd_ax
        self.pos = np.zeros(N, dtype=np.int64)
        cur = 1
        self.blocks = []
        L = int(level.max()) + 1 if N else 0
        if len(s_owner):
            order = np.argsort(s_owner, kind="stable")
            sm_s = s_member[order]
            so_s = s_owner[order]
            starts = np.searchsorted(so_s, np.arange(N))
            ends = np.searchsorted(so_s, np.arange(N), side="right")
        lvl_order = np.argsort(level, kind="stable")
        lb = np.searchsorted(level[lvl_order], np.arange(L + 1))
        for l in range(L):
            vs_l = lvl_order[lb[l]:lb[l + 1]]
            ax = vs_l[ntype[vs_l] == NT_AXIOM]
            if len(ax):
                n = len(ax); P = pow2(n)
                blk = Block()
                blk.kind, blk.n, blk.P, blk.Ptab = NT_AXIOM, n, P, pow2(cur)
                X = np.zeros((P, N_PHI_AX), dtype=np.float32)
                X[:n] = axn[ax]
                blk.X = Tensor(X)
                self.pos[ax] = cur + np.arange(n)
                cur += P
                self.blocks.append(blk)
            # every derived node of the level in ONE block: each inference
            # kind keeps its own batched Linear, selected by kind masks
            vs = vs_l[ntype[vs_l] != NT_AXIOM]
            if len(vs) == 0:
                continue
            n = len(vs); P = pow2(n)
            blk = Block()
            blk.kind, blk.n, blk.P, blk.Ptab = -1, n, P, pow2(cur)
            blk.i1 = self.idx_rows(self.gpos(in1[vs]), P)
            blk.i2 = self.idx_rows(self.gpos(in2[vs]), P)
            own, mem, w = [], [], []
            if len(s_owner):
                for k, v in enumerate(vs.tolist()):
                    for e in range(starts[v], ends[v]):
                        own.append(k); mem.append(sm_s[e])
                        w.append(1.0 / s_cnt[v])
            M = pow2(len(own))
            ow = np.zeros(M, dtype=np.int32); ow[:len(own)] = own
            me = np.full(M, -1, dtype=np.int64); me[:len(mem)] = mem
            ww = np.zeros((M, 1), dtype=np.float32); ww[:len(w), 0] = w
            blk.own = Tensor(np.repeat(ow[:, None], dim, axis=1))
            blk.mem = self.idx_rows(self.gpos(me), M)
            blk.w = Tensor(ww)
            oh = np.zeros((P, N_RULE_KINDS), dtype=np.float32)
            nk = ntype[vs]
            isr = nk == NT_RULE
            oh[np.nonzero(isr)[0], rkind[vs][isr]] = 1.0
            blk.onehot = Tensor(oh)
            for attr, t in (("m_cp", NT_CP), ("m_or", NT_RULE), ("m_sq", NT_SIMPQ)):
                m = np.zeros((P, 1), dtype=np.float32)
                m[:n, 0] = nk == t
                setattr(blk, attr, Tensor(m))
            self.pos[vs] = cur + np.arange(n)
            cur += P
            self.blocks.append(blk)
        self.n_table = cur
        self.Pfinal = pow2(cur)

    def gpos(self, arr):
        """Merged node index -> table row (-1 -> the 'missing' row 0)."""
        arr = np.asarray(arr, dtype=np.int64)
        return np.where(arr >= 0, self.pos[np.maximum(arr, 0)], 0)

    def idx_rows(self, ix, P):
        full = np.zeros(P, dtype=np.int32)
        full[:len(ix)] = ix
        return Tensor(np.repeat(full[:, None], self.dim, axis=1))


def row_parts(p, rr, off):
    """Split scored rows by how their embedding is formed: CP-kind rows
    from their two parents (merged node ids), the rest (queued axioms and
    requeued simplifications) are table nodes themselves."""
    nodes = p.cp_node[rr]
    kind = p.kind[rr]
    is_cp = (kind == TRACE_CP) | (nodes < 0)
    v = np.maximum(nodes, 0)
    pa = np.where((nodes >= 0) & (p.in1[v] >= 0), p.in1[v] + off, -1)
    pb = np.where((nodes >= 0) & (p.in2[v] >= 0), p.in2[v] + off, -1)
    node = np.where(nodes >= 0, nodes + off, -1)
    return is_cp, pa, pb, node


def balance_weights(y, rw, group, n_groups, pos_w=0.5):
    """Class-balanced per problem, every problem weighted equally."""
    w = np.zeros(len(y))
    for gi in range(n_groups):
        m = group == gi
        sp = rw[m & (y > 0)].sum(); sn = rw[m & (y == 0)].sum()
        w[m & (y > 0)] = rw[m & (y > 0)] * (pos_w / sp if sp > 0 else 0.0)
        w[m & (y == 0)] = rw[m & (y == 0)] * ((1 - pos_w) / sn if sn > 0 else 0.0)
    return w / max(1, n_groups)


class Plan:
    """Scoring layout for arbitrary rows (inference): CP-kind rows in
    chunked parent-gather blocks, the other rows gathered from the table.
    self.group / self.row map every output position back (row -1 = pad)."""

    def __init__(self, probs, rows, weights, std, dim, leaf_chunk=16384):
        self.layout = DagLayout(probs, std, dim)
        L = self.layout
        cp_g, cp_r, cp_a, cp_b, ot_g, ot_r, ot_n = [], [], [], [], [], [], []
        for gi, (p, rr) in enumerate(zip(probs, rows)):
            is_cp, pa, pb, node = row_parts(p, rr, L.offsets[gi])
            j = np.arange(len(rr))
            cp_g.append(np.full(int(is_cp.sum()), gi)); cp_r.append(j[is_cp])
            cp_a.append(pa[is_cp]); cp_b.append(pb[is_cp])
            ot_g.append(np.full(int((~is_cp).sum()), gi)); ot_r.append(j[~is_cp])
            ot_n.append(node[~is_cp])
        cp_g = np.concatenate(cp_g); cp_r = np.concatenate(cp_r)
        cp_a = np.concatenate(cp_a); cp_b = np.concatenate(cp_b)
        ot_g = np.concatenate(ot_g); ot_r = np.concatenate(ot_r)
        ot_n = np.concatenate(ot_n)
        grp, rix = [], []
        self.cp_blocks = []
        for c0 in range(0, len(cp_g), leaf_chunk):
            sl = slice(c0, c0 + leaf_chunk)
            n = len(cp_g[sl]); P = pow2(n)
            self.cp_blocks.append((L.idx_rows(L.gpos(cp_a[sl]), P),
                                   L.idx_rows(L.gpos(cp_b[sl]), P)))
            grp.append(np.concatenate([cp_g[sl], np.zeros(P - n, np.int64)]))
            rix.append(np.concatenate([cp_r[sl], np.full(P - n, -1)]))
        self.other_idx = None
        if len(ot_g):
            P = pow2(len(ot_g))
            self.other_idx = L.idx_rows(L.gpos(ot_n), P)
            grp.append(np.concatenate([ot_g, np.zeros(P - len(ot_g), np.int64)]))
            rix.append(np.concatenate([ot_r, np.full(P - len(ot_g), -1)]))
        self.group = np.concatenate(grp).astype(np.int64)
        self.row = np.concatenate(rix).astype(np.int64)
        real = self.row >= 0
        R = len(self.row)
        phi = np.zeros((R, N_PHI), dtype=np.float32)
        y = np.zeros(R, dtype=np.float32)
        rw = np.zeros(R, dtype=np.float64)
        for gi, (p, rr) in enumerate(zip(probs, rows)):
            m = real & (self.group == gi)
            src = rr[self.row[m]]
            phi[m] = (p.phi[src] - std.mu_phi) / std.sd_phi
            y[m] = p.label[src]
            rw[m] = weights[gi][self.row[m]]
        w = balance_weights(y, rw, self.group, len(probs))
        w[~real] = 0.0
        self.phi = Tensor(phi)
        self.y = Tensor(y)
        self.w = Tensor(w.astype(np.float32))
        self.n_real = int(real.sum())


class FixedBatch:
    """A training batch with FIXED shapes across steps, so its train step
    is captured once by TinyJit and replayed.  The batch's problems share
    one merged DAG layout (one block per level and inference kind for the
    whole batch; make_batches caps the merged table, since the one-hot
    gather cost grows with batch rows x batch table).  The layout and the
    non-CP rows are constants; each step feeds, per problem, every CP-kind
    positive plus K freshly sampled CP-kind negatives in a fixed segment
    of one index / feature / label / weight tensor."""

    def __init__(self, probs, std, dim, K):
        self.probs, self.std, self.K, self.dim = probs, std, K, dim
        self.layout = DagLayout(probs, std, dim)
        L = self.layout
        self.pos_rows, self.neg_pool = [], []
        ot_g, ot_rows, ot_n = [], [], []
        for gi, p in enumerate(probs):
            rr = np.nonzero(p.label | p.in_sample | (p.selected_at >= 0))[0]
            is_cp, pa, pb, node = row_parts(p, rr, L.offsets[gi])
            self.pos_rows.append(rr[is_cp & p.label[rr]])
            self.neg_pool.append(rr[is_cp & ~p.label[rr]])
            ot_g.append(np.full(int((~is_cp).sum()), gi))
            ot_rows.append(rr[~is_cp]); ot_n.append(node[~is_cp])
        self.ot_g = np.concatenate(ot_g)
        self.ot_rows = np.concatenate(ot_rows)
        ot_n = np.concatenate(ot_n)
        self.seg = np.array([len(x) + K for x in self.pos_rows])
        self.seg_off = np.concatenate([[0], np.cumsum(self.seg)])
        self.R_cp = pow2(int(self.seg.sum()))
        self.R_o = pow2(max(1, len(self.ot_g)))
        self.other_idx = L.idx_rows(L.gpos(ot_n), self.R_o)
        phi_o = np.zeros((self.R_o, N_PHI), dtype=np.float32)
        self.y_o = np.zeros(self.R_o, dtype=np.float32)
        for gi, p in enumerate(probs):
            m = np.nonzero(self.ot_g == gi)[0]
            phi_o[m] = (p.phi[self.ot_rows[m]] - std.mu_phi) / std.sd_phi
            self.y_o[m] = p.label[self.ot_rows[m]]
        self.phi_o = Tensor(phi_o)
        self.jit = None

    def sample(self, rng, weighting):
        """Numpy-built inputs of one step: CP-row parent indices (two),
        features, labels and weights (labels / weights cover the CP rows,
        then the other rows)."""
        L = self.layout
        B = len(self.probs)
        I1 = np.zeros(self.R_cp, np.int32); I2 = np.zeros_like(I1)
        PH = np.zeros((self.R_cp, N_PHI), np.float32)
        Y = np.zeros(self.R_cp + self.R_o, np.float32)
        RW = np.zeros(self.R_cp + self.R_o)
        G = np.full(self.R_cp + self.R_o, -1)
        self.last_rows = []
        for gi, p in enumerate(self.probs):
            neg = self.neg_pool[gi]
            if len(neg) > self.K:
                neg = rng.choice(neg, size=self.K, replace=False)
            rr = np.concatenate([self.pos_rows[gi], neg])
            self.last_rows.append(rr)
            _, pa, pb, _ = row_parts(p, rr, L.offsets[gi])
            c0, n = int(self.seg_off[gi]), len(rr)
            I1[c0:c0 + n] = L.gpos(pa); I2[c0:c0 + n] = L.gpos(pb)
            PH[c0:c0 + n] = (p.phi[rr] - self.std.mu_phi) / self.std.sd_phi
            Y[c0:c0 + n] = p.label[rr]
            RW[c0:c0 + n] = row_weights(p, rr, weighting)
            G[c0:c0 + n] = gi
        no = len(self.ot_g)
        Y[self.R_cp:self.R_cp + no] = self.y_o[:no]
        G[self.R_cp:self.R_cp + no] = self.ot_g
        for gi, p in enumerate(self.probs):
            m = np.nonzero(self.ot_g == gi)[0]
            RW[self.R_cp + m] = row_weights(p, self.ot_rows[m], weighting)
        W = balance_weights(Y, RW, G, B)
        W[G < 0] = 0.0
        d = self.dim
        return (Tensor(np.repeat(I1[:, None], d, 1)),
                Tensor(np.repeat(I2[:, None], d, 1)),
                Tensor(PH), Tensor(Y), Tensor(W.astype(np.float32)))


def act(x):
    return x.relu() - (x - 6.0).relu()


class HistoryNet:
    def __init__(self, dim=32, hid=32):
        self.dim, self.hid = dim, hid
        self.ax = nn.Linear(N_PHI_AX, dim)
        self.cp = nn.Linear(2 * dim, dim)
        self.orl = nn.Linear(2 * dim + N_RULE_KINDS, dim)
        self.sq = nn.Linear(2 * dim, dim)
        self.miss = Tensor.uniform(1, dim, low=-0.1, high=0.1)
        self.h1 = nn.Linear(dim + N_PHI, hid)
        self.out = nn.Linear(hid, 1)

    def table(self, layout):
        """Embeddings of every DAG node, level by level (padded rows)."""
        table = self.miss
        for b in layout.blocks:
            T = table.pad(((0, b.Ptab - table.shape[0]), None))
            if b.kind == NT_AXIOM:
                Z = self.ax(b.X)
            else:
                x1 = T.gather(0, b.i1)
                x2 = T.gather(0, b.i2)
                agg = Tensor.zeros(b.P, self.dim).scatter_reduce(
                    0, b.own, T.gather(0, b.mem) * b.w, reduce="sum")
                Z = (self.cp(x1.cat(x2, dim=1)) * b.m_cp
                     + self.orl(x1.cat(agg, b.onehot, dim=1)) * b.m_or
                     + self.sq(x1.cat(agg, dim=1)) * b.m_sq)
            table = table.cat(act(Z), dim=0)
        return table.pad(((0, layout.Pfinal - table.shape[0]), None))

    def score(self, T, cp_blocks, other_idx, phi):
        parts = [act(self.cp(T.gather(0, i1).cat(T.gather(0, i2), dim=1)))
                 for i1, i2 in cp_blocks]
        if other_idx is not None:
            parts.append(T.gather(0, other_idx))
        E = parts[0].cat(*parts[1:], dim=0) if len(parts) > 1 else parts[0]
        return self.out(self.h1(E.cat(phi, dim=1)).relu()).reshape(-1)

    def __call__(self, plan):
        T = self.table(plan.layout)
        return self.score(T, plan.cp_blocks, plan.other_idx, plan.phi)


def train_mode():
    """tinygrad's training switch: Tensor.train() on the PyPI releases,
    Context(TRAINING=1) on master (where Tensor.training is inert)."""
    if hasattr(Tensor, "train"):
        return Tensor.train()
    from tinygrad.helpers import Context
    return Context(TRAINING=1)


def plan_loss(model, plan):
    logit = model(plan)
    return (bce_rows(logit, plan.y) * plan.w).sum(), logit


def save_model(prefix, model, std, meta):
    safe_save(get_state_dict(model), prefix + ".safetensors")
    np.savez(prefix + "_std.npz", mu_phi=std.mu_phi, sd_phi=std.sd_phi,
             mu_ax=std.mu_ax, sd_ax=std.sd_ax, **meta)


def load_model(prefix):
    m = dict(np.load(prefix + "_std.npz"))
    model = HistoryNet(int(m["dim"]), int(m["hid"]))
    load_state_dict(model, safe_load(prefix + ".safetensors"))
    std = Std(arrays=(m["mu_phi"], m["sd_phi"], m["mu_ax"], m["sd_ax"]))
    return model, std, m


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


def row_weights(p, rr, weighting):
    """Per-row loss weights: uniform, or queue residence (selections the
    CP stays queued), which is how often it enters the gate's (step,
    queue) samples."""
    if weighting == "residence":
        end = np.minimum(p.end_step[rr], p.n_sel)
        return np.maximum(1, end - p.push_step[rr]).astype(np.float64)
    return np.ones(len(rr))


def make_batches(probs, core_cap=4096, max_n=64):
    """Fixed batches (one TinyJit capture each): problems of similar
    derivation depth merged while their summed core DAG stays under
    core_cap (a larger problem trains alone).  Blocks per batch = its
    deepest problem's levels x kinds."""
    order = sorted(range(len(probs)),
                   key=lambda i: (probs[i].n_levels, probs[i].n_core))
    out, cur, csum = [], [], 0
    for i in order:
        c = probs[i].n_core
        if cur and (csum + c > core_cap or len(cur) >= max_n):
            out.append(cur); cur, csum = [], 0
        cur.append(i); csum += c
    if cur:
        out.append(cur)
    return out


class PhiHead:
    """Features-only head (no derivation history): the warm start of the
    history net's scoring MLP, and the features-only baseline."""

    def __init__(self, hid):
        self.h1 = nn.Linear(N_PHI, hid)
        self.out = nn.Linear(hid, 1)

    def __call__(self, x):
        return self.out(self.h1(x).relu()).reshape(-1)


def phi_rows(probs, rng, K, std, weighting):
    X, Y, W = [], [], []
    for p in probs:
        pos = np.nonzero(p.label)[0]
        neg = np.nonzero(~p.label & (p.in_sample | (p.selected_at >= 0)))[0]
        if len(neg) > K:
            neg = rng.choice(neg, K, replace=False)
        rr = np.concatenate([pos, neg])
        y = p.label[rr].astype(np.float32)
        rw = row_weights(p, rr, weighting)
        w = balance_weights(y, rw, np.zeros(len(rr), np.int64), 1) / len(probs)
        X.append((p.phi[rr] - std.mu_phi) / std.sd_phi); Y.append(y); W.append(w)
    return np.concatenate(X), np.concatenate(Y), np.concatenate(W)


def train_phi_head(tr, std, hid, weighting, steps, rng, n_probs=64, K=256,
                   lr=3e-3):
    """Features-only head trained on many problems per step (a fixed
    padded shape, one TinyJit capture)."""
    head = PhiHead(hid)
    opt = nn.optim.Adam(get_parameters(head), lr=lr)
    R = pow2(n_probs * (K + 64))

    def step(x, y, w):
        loss = (bce_rows(head(x), y) * w).sum()
        opt.zero_grad(); loss.backward(); opt.step()
        return loss.realize()
    jit = TinyJit(step)
    with train_mode():
        for it in range(steps):
            idx = rng.choice(len(tr), min(n_probs, len(tr)), replace=False)
            X, Y, W = phi_rows([tr[i] for i in idx], rng, K, std, weighting)
            n = min(len(Y), R)
            Xp = np.zeros((R, N_PHI), np.float32); Xp[:n] = X[:n]
            Yp = np.zeros(R, np.float32); Yp[:n] = Y[:n]
            Wp = np.zeros(R, np.float32); Wp[:n] = W[:n]
            loss = jit(Tensor(Xp).realize(), Tensor(Yp).realize(), Tensor(Wp).realize())
            if it % 100 == 0:
                print(f"  phi head step {it} loss {loss.item():.4f}", flush=True)
    return head


def warm_start(model, head):
    """History net whose scoring MLP starts as the features-only head:
    the embedding columns of h1 start at zero."""
    W = np.zeros((model.hid, model.dim + N_PHI), np.float32)
    W[:, model.dim:] = head.h1.weight.numpy()
    model.h1.weight.assign(Tensor(W)).realize()
    model.h1.bias.assign(Tensor(head.h1.bias.numpy())).realize()
    model.out.weight.assign(Tensor(head.out.weight.numpy())).realize()
    model.out.bias.assign(Tensor(head.out.bias.numpy())).realize()


def batch_logits(model, fb, i1, i2, ph):
    """Logits of a FixedBatch: its CP rows, then its other rows."""
    T = model.table(fb.layout)
    return model.score(T, [(i1, i2)], fb.other_idx, ph.cat(fb.phi_o, dim=0))


def bce_rows(logit, y):
    """Per-row binary cross-entropy from a logit, as a 2-class
    log_softmax over [0, logit] (log_softmax is in py/thvm; tinygrad's
    binary_crossentropy_logits / logsigmoid are not)."""
    n = logit.shape[0]
    ls = Tensor.zeros(n, 1).cat(logit.reshape(n, 1), dim=1).log_softmax(1)
    return -(ls[:, 1] * y + ls[:, 0] * (1.0 - y))


def batch_loss(model, fb, i1, i2, ph, y, w):
    logit = batch_logits(model, fb, i1, i2, ph)
    return (bce_rows(logit, y) * w).sum()


def cmd_train(npz_dir, pool_path, out_prefix, dim=32, hid=32, epochs=60,
              seed=0, K=256, lr=3e-3, patience=8, weighting="uniform",
              max_levels=100, warm_steps=0):
    split = load_pool(pool_path)
    names = sorted(n for n, s in split.items() if s == "train")
    tr_names = [n for n in names if not is_val(n)]
    va_names = [n for n in names if is_val(n)]
    t0 = time.time()
    # Training (and the early-stopping loss) skips the few problems whose
    # derivation DAG is deeper than max_levels: one captured step per
    # such problem, for little data.  The gates score every problem.
    tr_all = load_problems(npz_dir, tr_names)
    va_all = load_problems(npz_dir, va_names)
    tr = [p for p in tr_all if p.n_levels <= max_levels]
    va = [p for p in va_all if p.n_levels <= max_levels]
    print(f"depth cap {max_levels}: train {len(tr)}/{len(tr_all)} "
          f"val {len(va)}/{len(va_all)}", flush=True)
    Tensor.manual_seed(seed)
    rng = np.random.default_rng(seed)
    std = Std(tr)
    model = HistoryNet(dim, hid)
    if warm_steps > 0:
        head = train_phi_head(tr, std, hid, weighting, warm_steps, rng)
        warm_start(model, head)
        save_model(out_prefix + "_phi", model, std,
                   dict(dim=dim, hid=hid, weighting=weighting, seed=seed,
                        warm_steps=warm_steps, features_only=1))
    opt = nn.optim.Adam(get_parameters(model), lr=lr)
    tb = [FixedBatch([tr[i] for i in b], std, dim, K) for b in make_batches(tr)]
    vb = [FixedBatch([va[i] for i in b], std, dim, K) for b in make_batches(va)]
    print(f"loaded train={len(tr)} ({len(tb)} batches) val={len(va)} "
          f"({len(vb)} batches) in {time.time()-t0:.0f}s; levels max="
          f"{max(p.n_levels for p in tr)} core max={max(p.n_core for p in tr)}",
          flush=True)
    for fb in tb:
        def step(i1, i2, ph, y, w, fb=fb):
            loss = batch_loss(model, fb, i1, i2, ph, y, w)
            opt.zero_grad()
            loss.backward()
            opt.step()
            return loss.realize()
        fb.jit = TinyJit(step)
    vrng = np.random.default_rng(1234)
    for fb in vb:
        def fwd(i1, i2, ph, y, w, fb=fb):
            return batch_loss(model, fb, i1, i2, ph, y, w).realize()
        fb.jit = TinyJit(fwd)
        fb.fixed = [x.realize() for x in fb.sample(vrng, weighting)]
    ntr = sum(len(fb.probs) for fb in tb); nva = sum(len(fb.probs) for fb in vb)
    meta = dict(dim=dim, hid=hid, weighting=weighting, seed=seed, lr=lr, K=K,
                max_levels=max_levels, warm_steps=warm_steps)
    best = (np.inf, -1)
    for ep in range(epochs):
        te = time.time()
        tl = 0.0
        with train_mode():
            for bi in rng.permutation(len(tb)):
                fb = tb[bi]
                tb0 = time.time()
                ins = [x.realize() for x in fb.sample(rng, weighting)]
                tl += fb.jit(*ins).item() * len(fb.probs)
                if ep < 2:
                    print(f"  batch {bi}: {len(fb.probs)} problems, "
                          f"{len(fb.layout.blocks)} blocks, table "
                          f"{fb.layout.Pfinal}, rows {fb.R_cp}+{fb.R_o}: "
                          f"{time.time()-tb0:.1f}s", flush=True)
        vl = sum(fb.jit(*fb.fixed).item() * len(fb.probs) for fb in vb) / nva
        print(f"epoch {ep} train {tl/ntr:.4f} val {vl:.4f} "
              f"({time.time()-te:.0f}s)", flush=True)
        if vl < best[0]:
            best = (vl, ep)
            save_model(out_prefix, model, std,
                       dict(meta, best_epoch=ep, best_val=vl))
        elif ep - best[1] >= patience:
            break
    print(f"saved {out_prefix} best epoch {best[1]} val {best[0]:.4f}")


# --------------------------------------------------------------------------
# pre-registered gate (gate_spec.txt)

def score_problem(model, std, p, leaf_chunk=16384):
    """Logit for every kept CP of problem p, in table order."""
    rr = np.arange(len(p.seq))
    plan = Plan([p], [rr], [np.ones(len(rr))], std, model.dim, leaf_chunk)
    logit = model(plan).numpy()
    out = np.zeros(len(rr))
    real = plan.row >= 0
    out[plan.row[real]] = logit[real]
    return out


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


def candidate_steps(p):
    """Selections t with some positive pending (push <= t-1, selected >= t):
    pending count at t = #{push <= t-1} - #{selected <= t-1}."""
    pos_idx = np.nonzero(p.label)[0]
    T = p.n_sel
    ps, pe = p.push_step[pos_idx], p.selected_at[pos_idx]
    ev = np.zeros(T + 2, dtype=np.int64)
    np.add.at(ev, np.clip(ps + 1, 0, T + 1), 1)
    np.add.at(ev, np.clip(pe + 1, 0, T + 1), -1)
    pending = np.cumsum(ev)
    return np.nonzero(pending[1:T + 1] > 0)[0] + 1


def gate_problem(p, logit, max_steps=50):
    """Gate statistics of one problem.  logit: one score per kept CP (fixed
    at push), or a function (t, members) -> the members' scores at
    selection t."""
    if p.label.sum() == 0 or p.n_sel < 2:
        return None
    cand = candidate_steps(p)
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
        if callable(logit):
            sc = logit(t, np.concatenate([P_t, N_t]))
            model_kp, model_kn = -sc[:len(P_t)], -sc[len(P_t):]
        else:
            model_kp, model_kn = -logit[P_t], -logit[N_t]
        keys = {"model": (model_kp, model_kn),
                "engine": (pri_t[P_t].astype(np.float64), pri_t[N_t].astype(np.float64)),
                "weight": (p.weight[P_t].astype(np.float64), p.weight[N_t].astype(np.float64))}
        for rk, (kp, kn) in keys.items():
            # lexicographic (key, seq): encode as sort order
            sp = p.seq[P_t]
            sn = p.seq[N_t]
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


def cmd_gate(model_prefix, npz_dir, pool_path, out_prefix, which="heldout"):
    model, std, _ = load_model(model_prefix)
    split = load_pool(pool_path)
    if which == "heldout":
        names = sorted(n for n, s in split.items() if s == "heldout")
    else:   # the validation slice carved from train (model selection only)
        names = sorted(n for n, s in split.items() if s == "train" and is_val(n))
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
        logit = score_problem(model, std, p)
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
# sanity: the optimizer moves the parameters, the loss falls, and a batch
# scoring a row subset agrees with the full-problem scoring

def cmd_sanity(npz_dir, pool_path, n_probs=6, steps=30, dim=16, hid=16):
    split = load_pool(pool_path)
    names = sorted(n for n, s in split.items() if s == "train" and not is_val(n))
    probs = load_problems(npz_dir, names[:40])
    probs = sorted(probs, key=lambda p: -p.n_core)[:n_probs]
    Tensor.manual_seed(0)
    rng = np.random.default_rng(0)
    std = Std(probs)
    model = HistoryNet(dim, hid)
    fb = FixedBatch(probs, std, dim, 64)
    i1, i2, ph, y, w = fb.sample(rng, "uniform")
    lg = batch_logits(model, fb, i1, i2, ph).numpy()
    # the same rows through the inference layout
    rows = [np.concatenate([rr, fb.ot_rows[fb.ot_g == gi]])
            for gi, rr in enumerate(fb.last_rows)]
    plan = Plan(probs, rows, [np.ones(len(r)) for r in rows], std, dim)
    lp = model(plan).numpy()
    ref = {(g, rows[g][r]): v for g, r, v in zip(plan.group, plan.row, lp) if r >= 0}
    got = []
    for gi, rr in enumerate(fb.last_rows):
        c0 = int(fb.seg_off[gi])
        for k, r in enumerate(rr.tolist()):
            got.append(abs(lg[c0 + k] - ref[(gi, r)]))
    for k, (gi, r) in enumerate(zip(fb.ot_g.tolist(), fb.ot_rows.tolist())):
        got.append(abs(lg[fb.R_cp + k] - ref[(gi, r)]))
    print(f"{len(probs)} problems (core {[p.n_core for p in probs]}): "
          f"fixed-batch vs inference layout max |diff| {max(got):.2e} "
          f"over {len(got)} rows")
    params = get_parameters(model)
    before = [x.numpy().copy() for x in params]
    opt = nn.optim.Adam(params, lr=5e-3)

    def step(i1, i2, ph, y, w):
        loss = batch_loss(model, fb, i1, i2, ph, y, w)
        opt.zero_grad(); loss.backward(); opt.step()
        return loss.realize()
    jit = TinyJit(step)
    fixed = [x.realize() for x in fb.sample(rng, "uniform")]
    losses, times = [], []
    with train_mode():
        for it in range(steps):
            t0 = time.time()
            losses.append(jit(*fixed).item())
            times.append(time.time() - t0)
    moved = max(float(np.max(np.abs(x.numpy() - b0)))
                for x, b0 in zip(params, before))
    print(f"loss {losses[0]:.4f} -> {losses[-1]:.4f} over {steps} jitted Adam "
          f"steps; max |param delta| {moved:.3e}; step s: first "
          f"{times[0]:.1f} second {times[1]:.1f} replay {np.median(times[2:]):.3f}; "
          f"{'OK' if losses[-1] < 0.8 * losses[0] and moved > 0 else 'FAIL'}")


def cmd_recon(npz_dir, pool_path, out_path, max_probs=10 ** 9):
    """Queue-reconstruction check: at every selection t, rank the CP the
    engine actually picked inside the rebuilt queue Q_t under the engine
    key (cp_pri at t, seq) and under pure FIFO (seq).  A faithful rebuild
    puts the pick at rank 0 under one of them (weight pick / FIFO pick;
    ultimate axioms first).  Exact problems (rate 1) only."""
    split = load_pool(pool_path)
    names = sorted(split)
    tot = {"n": 0, "w0": 0, "f0": 0, "ult": 0, "other": 0}
    worst = []
    done = 0
    for nm in names:
        path = os.path.join(npz_dir, nm + ".npz")
        if not os.path.exists(path) or done >= max_probs:
            continue
        d = np.load(path)
        if float(d["meta"][0]) < 1.0:
            continue
        done += 1
        seq = d["seq"].astype(np.int64)
        push = d["push_step"].astype(np.int64)
        end = d["end_step"].astype(np.int64)
        sel = d["selected_at"].astype(np.int64)
        pri0 = np.where(d["has_push"], d["pri_push"], d["pri"]).astype(np.int64)
        ult = (d["flags"].astype(np.int64) & 1) != 0
        ws, wst, wp = (d["w_seq"].astype(np.int64), d["w_step"].astype(np.int64),
                       d["w_pri"].astype(np.int64))
        wi = np.searchsorted(seq, ws)
        T = int(d["meta"][2])
        order_sel = np.argsort(sel)
        pri = pri0.copy()
        wptr = 0
        other = 0
        for t in range(1, T + 1):
            while wptr < len(wst) and wst[wptr] <= t - 1:
                if wi[wptr] < len(seq) and seq[wi[wptr]] == ws[wptr]:
                    pri[wi[wptr]] = wp[wptr]
                wptr += 1
            j = np.nonzero(sel == t)[0]
            if len(j) != 1:
                continue
            j = j[0]
            alive = (push <= t - 1) & (end >= t)
            tot["n"] += 1
            if ult[j]:
                tot["ult"] += 1
                continue
            a = np.nonzero(alive & ~ult)[0]
            kp, ks = pri[a], seq[a]
            wrank = int(np.sum((kp < pri[j]) | ((kp == pri[j]) & (ks < seq[j]))))
            frank = int(np.sum(ks < seq[j]))
            if wrank == 0:
                tot["w0"] += 1
            elif frank == 0:
                tot["f0"] += 1
            else:
                tot["other"] += 1
                other += 1
        worst.append((other, nm, T))
    worst.sort(reverse=True)
    lines = [f"problems {done} selections {tot['n']}: weight-rank-0 {tot['w0']} "
             f"fifo-rank-0 {tot['f0']} ultimate {tot['ult']} other {tot['other']}",
             "most 'other' picks: " + ", ".join(f"{nm} {o}/{T}" for o, nm, T in worst[:10])]
    txt = "\n".join(lines)
    print(txt)
    with open(out_path, "w") as f:
        f.write(txt + "\n")


def main():
    cmd = sys.argv[1]
    if cmd == "train":
        a = sys.argv[2:]
        kw = {}
        if len(a) > 3: kw["dim"] = int(a[3])
        if len(a) > 4: kw["hid"] = int(a[4])
        if len(a) > 5: kw["epochs"] = int(a[5])
        if len(a) > 6: kw["seed"] = int(a[6])
        if len(a) > 7: kw["weighting"] = a[7]
        if len(a) > 8: kw["warm_steps"] = int(a[8])
        if len(a) > 9: kw["lr"] = float(a[9])
        cmd_train(a[0], a[1], a[2], **kw)
    elif cmd == "gate":
        cmd_gate(*sys.argv[2:6])
    elif cmd == "valgate":
        cmd_gate(*sys.argv[2:6], which="val")
    elif cmd == "sanity":
        cmd_sanity(sys.argv[2], sys.argv[3])
    elif cmd == "summary":
        cmd_summary(*sys.argv[2:5])
    elif cmd == "recon":
        cmd_recon(sys.argv[2], sys.argv[3], sys.argv[4],
                  int(sys.argv[5]) if len(sys.argv) > 5 else 10 ** 9)


if __name__ == "__main__":
    main()
