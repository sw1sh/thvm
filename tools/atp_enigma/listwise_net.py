#!/usr/bin/env python3
# Neural clause selection, phase 0b: the phase-0 derivation-history
# network (history_net.py) trained with a LISTWISE objective over each
# selection step's reconstructed queue, with the engine's own signal as
# an input, plus the pre-registered phase-0b gate
# (~/.cache/thvm/casc/J13/neural_p0/gate_spec_0b.txt).
#
# Score of critical pair (CP) c at selection step t:
#   s(c, t) = -kappa log1p(rhat_t(c))
#             + W2 relu(W1 [E(c) ; phi(c)] + b1 + We e(c, t)) + b2
# E(c) is the phase-0 derivation-history embedding (absent in the head
# variant), phi(c) phase 0's 28 features, e(c, t) the engine signal at t,
# computed without labels: rhat_t(c) = the sampled queue members before c
# under the engine key (cp_pri at t, seq), each weighing 1/rate; the same
# over the queue size; log1p(cp_pri at t).  W2 and b2 start at zero, so
# the untrained model ranks by the engine rank and training learns a
# correction.
#
# Objective: at a sampled step t, -log of the softmax mass of the pending
# proof pairs P_t within the queue Q_t (positives weigh 1, sampled
# negatives 1/rate), the supervised form of [Sud25]'s policy gradient.
# The negatives entering the softmax are each drawn step's engine top
# H_TOP (weight 1/rate) plus K_U drawn uniformly from the rest of the
# drawn steps' queues (weight 1/(rate q)), so every step's denominator is
# an unbiased estimate of the full queue's.  A batch's rows belong to one
# problem each, and step slot k is the k-th drawn step of every problem:
# the per-(step, problem) log-sum-exp is a scatter_reduce over the
# problem index (amax for the shift, sum for the mass).
#
# Ops (the tinygrad API py/thvm mirrors): nn.Linear, relu, Tensor.gather,
# Tensor.scatter_reduce (sum, amax), exp / log, nn.optim.Adam, TinyJit,
# Context(TRAINING=1).
#
# Usage:
#   listwise_net.py train   <npz_dir> <pool.tsv> <out_prefix> <H32|N16|N32>
#   listwise_net.py valgate <model_prefix> <npz_dir> <pool.tsv> <out_prefix>
#   listwise_net.py gate    <model_prefix> <npz_dir> <pool.tsv> <out_prefix>
#   listwise_net.py sanity  <npz_dir> <pool.tsv>
import os
import sys
import time

import numpy as np
from tinygrad import Tensor, TinyJit, nn
from tinygrad.nn.state import get_parameters, get_state_dict, safe_load, safe_save, load_state_dict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from history_net import (N_PHI, N_PHI_AX, N_RULE_KINDS, DagLayout, HistoryNet, Plan, Problem,
                         Std, act, bootstrap, candidate_steps, engine_pri_at, fix_levels,
                         gate_problem, is_val, load_pool, load_problems, make_batches, pow2,
                         row_parts, train_mode, validate_levels)

N_ENG = 3
S_STEPS = 16     # steps drawn per problem per sample
H_TOP = 8        # engine top negatives per drawn step
K_U = 64         # uniform negatives per problem
BIG = 1e4        # log-weight of a non-member (exp underflows to 0)
VARIANTS = {"H32": (0, 32, False), "N16": (16, 16, True), "N32": (32, 32, True)}


# --------------------------------------------------------------------------
# queues and the engine signal

def step_view(p, t):
    """Queue at selection t: alive mask, cp_pri at t, the engine key
    (cp_pri, seq) packed in a u64, and the sorted keys of the sampled
    members (the label-free queue estimate)."""
    alive = (p.push_step <= t - 1) & (p.end_step >= t)
    if len(p.w_seq):
        pri_t = engine_pri_at(p, t)
        key = (pri_t.astype(np.uint64) << np.uint64(32)) | p.seq.astype(np.uint64)
    else:
        pri_t = p.pri
        if not hasattr(p, "key0"):
            p.key0 = (p.pri.astype(np.uint64) << np.uint64(32)) | p.seq.astype(np.uint64)
        key = p.key0
    return alive, pri_t, key, np.sort(key[alive & p.in_sample])


def eng_feats(p, view, idx):
    """Engine signal of CPs idx at the view's step, and log1p(rhat)."""
    _, pri_t, key, qk = view
    rh = np.searchsorted(qk, key[idx], side="left") / p.rate
    sh = max(len(qk) / p.rate, 1.0)
    lr = np.log1p(rh)
    return np.stack([lr, rh / sh, np.log1p(pri_t[idx].astype(np.float64))], 1), lr


def sample_problem(p, cand, rng, S=S_STEPS, h=H_TOP, k_u=K_U):
    """One draw: steps, per-step views, rows (positives, engine-top
    negatives, uniform negatives) and each row's log-weight."""
    steps = np.sort(rng.choice(cand, S, replace=False)) if len(cand) > S else cand
    negm = p.in_sample & ~p.label
    views, tops = [], []
    pool = np.zeros(len(p.seq), dtype=bool)
    for t in steps:
        v = step_view(p, t)
        n_t = v[0] & negm
        pool |= n_t
        neg = np.nonzero(n_t)[0]
        if len(neg) > h:
            neg = neg[np.argpartition(v[2][neg], h)[:h]]
        tops.append(neg)
        views.append(v)
    top = np.unique(np.concatenate(tops))
    pool[top] = False
    rest = np.nonzero(pool)[0]
    if len(rest) > k_u:
        uni = np.sort(rng.choice(rest, k_u, replace=False))
        q = k_u / len(rest)
    else:
        uni, q = rest, 1.0
    pos = np.nonzero(p.label)[0]
    rows = np.concatenate([pos, top, uni])
    lw = np.concatenate([np.zeros(len(pos)), np.full(len(top), -np.log(p.rate)),
                         np.full(len(uni), -np.log(p.rate * q))])
    return steps, views, rows, lw


def pair_arrays(p, views, rows, lw, S=S_STEPS):
    """Per (step slot, row): log-weight (-BIG off the queue), positive
    log-mask, engine features, log1p(rhat).  Slots past the drawn steps
    repeat them (their loss weight is zero)."""
    n = len(rows)
    LW = np.full((S, n), -BIG); PM = np.full((S, n), -BIG)
    ENG = np.zeros((S, n, N_ENG)); RAW = np.zeros((S, n))
    lab = p.label[rows]
    for k in range(S):
        j = k % len(views)
        if k >= len(views):
            LW[k], PM[k], ENG[k], RAW[k] = LW[j], PM[j], ENG[j], RAW[j]
            continue
        a = views[j][0][rows]
        ENG[k], RAW[k] = eng_feats(p, views[j], rows)
        LW[k] = np.where(a, lw, -BIG)
        PM[k] = np.where(a & lab, 0.0, -BIG)
    return LW, PM, ENG, RAW


def eng_stats(probs, rng):
    """Mean / sd of the engine features over queue members of one draw
    per problem."""
    xs = []
    for p in probs:
        cand = candidate_steps(p)
        steps, views, rows, lw = sample_problem(p, cand, rng)
        LW, _, ENG, _ = pair_arrays(p, views, rows, lw)
        xs.append(ENG[:len(steps)][LW[:len(steps)] > -BIG / 2])
    x = np.concatenate(xs)
    return x.mean(0), x.std(0) + 1e-6


# --------------------------------------------------------------------------
# batches and model

class ListBatch:
    """A training batch with FIXED shapes across draws (one TinyJit
    capture): problem b owns rows [off_b, off_b + n_pos_b + S H_TOP + K_U),
    the rest padding; step slot k of the batch is every problem's k-th
    drawn step.  With history, the problems' core DAGs share one layout."""

    def __init__(self, probs, std, dim, history):
        self.probs, self.std, self.dim, self.history = probs, std, dim, history
        self.layout = DagLayout(probs, std, dim) if history else None
        self.cand = [candidate_steps(p) for p in probs]
        self.seg = np.array([int(p.label.sum()) + S_STEPS * H_TOP + K_U for p in probs])
        self.off = np.concatenate([[0], np.cumsum(self.seg)])
        self.R = pow2(int(self.seg.sum()))
        self.jit = None

    def draw(self, rng):
        """Numpy inputs of one step (see ListNet.loss for their order)."""
        B, R, S, d = len(self.probs), self.R, S_STEPS, max(1, self.dim)
        I1 = np.zeros(R, np.int32); I2 = np.zeros(R, np.int32)
        MCP = np.zeros((R, 1), np.float32); PHI = np.zeros((R, N_PHI), np.float32)
        ENG = np.zeros((S, R, N_ENG), np.float32); RAW = np.zeros((S, R), np.float32)
        LW = np.full((S, R), -BIG, np.float32); PM = np.full((S, R), -BIG, np.float32)
        SEG = np.zeros((S, R), np.int32); SW = np.zeros((S, B), np.float32)
        self.last = []
        for b, p in enumerate(self.probs):
            steps, views, rows, lw = sample_problem(p, self.cand[b], rng)
            lwa, pma, enga, rawa = pair_arrays(p, views, rows, lw)
            c0, n = int(self.off[b]), len(rows)
            sl = slice(c0, c0 + n)
            if self.history:
                is_cp, pa, pb, node = row_parts(p, rows, self.layout.offsets[b])
                I1[sl] = self.layout.gpos(np.where(is_cp, pa, node))
                I2[sl] = np.where(is_cp, self.layout.gpos(pb), 0)
                MCP[sl, 0] = is_cp
            PHI[sl] = (p.phi[rows] - self.std.mu_phi) / self.std.sd_phi
            ENG[:, sl] = (enga - self.std.mu_eng) / self.std.sd_eng
            RAW[:, sl] = rawa; LW[:, sl] = lwa; PM[:, sl] = pma
            SEG[:, int(self.off[b]):int(self.off[b + 1])] = b
            SW[:len(steps), b] = 1.0 / (B * len(steps))
            self.last.append((steps, rows, lwa, pma))
        return (np.repeat(I1[:, None], d, 1), np.repeat(I2[:, None], d, 1), MCP, PHI,
                ENG, RAW, LW, PM, SEG, SW)

    def sample(self, rng):
        return [Tensor(x).realize() for x in self.draw(rng)]


class ListNet:
    def __init__(self, dim, hid, history):
        self.dim, self.hid, self.history = dim, hid, history
        if history:
            self.ax = nn.Linear(N_PHI_AX, dim)
            self.cp = nn.Linear(2 * dim, dim)
            self.orl = nn.Linear(2 * dim + N_RULE_KINDS, dim)
            self.sq = nn.Linear(2 * dim, dim)
            self.miss = Tensor.uniform(1, dim, low=-0.1, high=0.1)
        self.h1 = nn.Linear((dim if history else 0) + N_PHI, hid)
        self.he = nn.Linear(N_ENG, hid, bias=False)
        self.out = nn.Linear(hid, 1)
        # numpy-backed: tinygrad 0.11 hash-conses Tensor.zeros(...).contiguous(),
        # so a parameter built that way shares its buffer with every
        # same-shaped optimizer state (Adam's m, v, b1_t, b2_t are built the
        # same way) once a forward pass realizes it
        self.out.weight = Tensor(np.zeros((1, hid), np.float32))
        self.out.bias = Tensor(np.zeros(1, np.float32))
        self.kappa = Tensor(np.ones(1, np.float32))

    table = HistoryNet.table

    def rows(self, T, i1, i2, mcp, phi):
        """Per-row hidden pre-activation W1 [E ; phi] + b1."""
        if not self.history:
            return self.h1(phi)
        x1 = T.gather(0, i1)
        E = act(self.cp(x1.cat(T.gather(0, i2), dim=1))) * mcp + x1 * (1.0 - mcp)
        return self.h1(E.cat(phi, dim=1))

    def pairs(self, Z, eng, raw):
        S, R = raw.shape
        H = (Z.reshape(1, R, self.hid) + self.he(eng)).relu()
        return self.out(H).reshape(S, R) - self.kappa * raw

    def scores(self, fb, i1, i2, mcp, phi, eng, raw):
        T = self.table(fb.layout) if self.history else None
        return self.pairs(self.rows(T, i1, i2, mcp, phi), eng, raw)

    def loss(self, fb, i1, i2, mcp, phi, eng, raw, lw, pm, seg, sw):
        return list_loss(self.scores(fb, i1, i2, mcp, phi, eng, raw), lw, pm, seg, sw)


def list_loss(s, lw, pm, seg, sw):
    """sum over (step k, problem b) of sw[k, b] (lse over b's queue at k
    - lse over b's pending positives at k): one segment log-sum-exp per
    queue, shifted by the segment max (no gradient through the shift)."""
    S, B = sw.shape
    X = s + lw
    Xp = s + pm
    m = Tensor.full((S, B), -BIG).scatter_reduce(1, seg, X.detach(), "amax")
    mp = Tensor.full((S, B), -BIG).scatter_reduce(1, seg, Xp.detach(), "amax")
    den = Tensor.zeros(S, B).scatter_reduce(1, seg, (X - m.gather(1, seg)).exp(), "sum")
    num = Tensor.zeros(S, B).scatter_reduce(1, seg, (Xp - mp.gather(1, seg)).exp(), "sum")
    return (sw * ((den + 1e-30).log() + m - (num + 1e-30).log() - mp)).sum()


def list_loss_ref(s, lw, pm, sw, off):
    """numpy reference of list_loss (s: [S, R] scores)."""
    tot = 0.0
    S, B = sw.shape
    for b in range(B):
        sl = slice(int(off[b]), int(off[b + 1]))
        for k in range(S):
            if sw[k, b] == 0:
                continue
            x = s[k, sl].astype(np.float64)
            wa = np.where(lw[k, sl] > -BIG / 2, np.exp(lw[k, sl].astype(np.float64)), 0.0)
            wp = (pm[k, sl] > -BIG / 2).astype(np.float64)
            mx = x.max()
            tot += sw[k, b] * (np.log(np.sum(wa * np.exp(x - mx))) - np.log(np.sum(wp * np.exp(x - mx))))
    return tot


def save_model(prefix, model, std, meta):
    safe_save(get_state_dict(model), prefix + ".safetensors")
    np.savez(prefix + "_std.npz", mu_phi=std.mu_phi, sd_phi=std.sd_phi, mu_ax=std.mu_ax,
             sd_ax=std.sd_ax, mu_eng=std.mu_eng, sd_eng=std.sd_eng, **meta)


def load_model(prefix):
    m = dict(np.load(prefix + "_std.npz"))
    model = ListNet(int(m["dim"]), int(m["hid"]), bool(m["history"]))
    load_state_dict(model, safe_load(prefix + ".safetensors"))
    std = Std(arrays=(m["mu_phi"], m["sd_phi"], m["mu_ax"], m["sd_ax"]))
    std.mu_eng, std.sd_eng = m["mu_eng"], m["sd_eng"]
    return model, std, m


# --------------------------------------------------------------------------
# training

def split_problems(npz_dir, pool_path, max_levels):
    split = load_pool(pool_path)
    names = sorted(n for n, s in split.items() if s == "train")
    out = []
    for nm in ([n for n in names if not is_val(n)], [n for n in names if is_val(n)]):
        ps = load_problems(npz_dir, nm)
        out.append([p for p in ps if p.n_levels <= max_levels and len(candidate_steps(p))])
    return out


def cmd_train(npz_dir, pool_path, out_prefix, variant, epochs=30, patience=5, lr=1e-3,
              seed=0, max_levels=100):
    dim, hid, history = VARIANTS[variant]
    t0 = time.time()
    tr, va = split_problems(npz_dir, pool_path, max_levels)
    Tensor.manual_seed(seed)
    rng = np.random.default_rng(seed)
    std = Std(tr)
    std.mu_eng, std.sd_eng = eng_stats(tr, np.random.default_rng(999))
    model = ListNet(dim, hid, history)
    opt = nn.optim.Adam(get_parameters(model), lr=lr)
    tb = [ListBatch([tr[i] for i in b], std, dim, history) for b in make_batches(tr)]
    vb = [ListBatch([va[i] for i in b], std, dim, history) for b in make_batches(va)]
    print(f"{variant}: train {len(tr)} problems ({len(tb)} batches) val {len(va)} "
          f"({len(vb)} batches), loaded in {time.time()-t0:.0f}s; engine features mu "
          f"{np.round(std.mu_eng, 3).tolist()} sd {np.round(std.sd_eng, 3).tolist()}", flush=True)
    for fb in tb:
        def step(*ins, fb=fb):
            loss = model.loss(fb, *ins)
            opt.zero_grad()
            loss.backward()
            opt.step()
            return loss.realize()
        fb.jit = TinyJit(step)
    vrng = np.random.default_rng(1234)
    for fb in vb:
        def fwd(*ins, fb=fb):
            return model.loss(fb, *ins).realize()
        fb.jit = TinyJit(fwd)
        fb.fixed = fb.sample(vrng)
    ntr = sum(len(fb.probs) for fb in tb); nva = sum(len(fb.probs) for fb in vb)
    meta = dict(dim=dim, hid=hid, history=int(history), variant=variant, seed=seed, lr=lr,
                S=S_STEPS, h=H_TOP, k_u=K_U, max_levels=max_levels)
    vl0 = sum(fb.jit(*fb.fixed).item() * len(fb.probs) for fb in vb) / nva
    print(f"epoch -1 (the engine rank alone, kappa 1) val {vl0:.4f}", flush=True)
    best = (np.inf, -1)
    for ep in range(epochs):
        te = time.time()
        tl = 0.0
        with train_mode():
            for bi in rng.permutation(len(tb)):
                fb = tb[bi]
                tb0 = time.time()
                tl += fb.jit(*fb.sample(rng)).item() * len(fb.probs)
                if ep < 1:
                    print(f"  batch {bi}: {len(fb.probs)} problems, rows {fb.R}"
                          + (f", {len(fb.layout.blocks)} blocks, table {fb.layout.Pfinal}"
                             if history else "") + f": {time.time()-tb0:.1f}s", flush=True)
        vl = sum(fb.jit(*fb.fixed).item() * len(fb.probs) for fb in vb) / nva
        print(f"epoch {ep} train {tl/ntr:.4f} val {vl:.4f} kappa "
              f"{model.kappa.numpy()[0]:.3f} ({time.time()-te:.0f}s)", flush=True)
        if vl < best[0]:
            best = (vl, ep)
            save_model(out_prefix, model, std, dict(meta, best_epoch=ep, best_val=vl))
        elif ep - best[1] >= patience:
            break
    print(f"saved {out_prefix} best epoch {best[1]} val {best[0]:.4f}")


# --------------------------------------------------------------------------
# gate (gate_spec_0b.txt): per-step scores in numpy from the trained weights

def row_hidden_all(model, std, p, leaf_chunk=16384):
    """W1 [E(c) ; phi(c)] + b1 for every kept CP of p."""
    phi = (p.phi - std.mu_phi) / std.sd_phi
    W1 = model.h1.weight.numpy().astype(np.float64)
    b1 = model.h1.bias.numpy().astype(np.float64)
    if not model.history:
        return phi @ W1.T + b1
    rr = np.arange(len(p.seq))
    plan = Plan([p], [rr], [np.ones(len(rr))], std, model.dim, leaf_chunk)
    T = model.table(plan.layout)
    parts = [act(model.cp(T.gather(0, i1).cat(T.gather(0, i2), dim=1)))
             for i1, i2 in plan.cp_blocks]
    if plan.other_idx is not None:
        parts.append(T.gather(0, plan.other_idx))
    E = (parts[0].cat(*parts[1:], dim=0) if len(parts) > 1 else parts[0]).numpy()
    real = plan.row >= 0
    Eall = np.zeros((len(rr), model.dim))
    Eall[plan.row[real]] = E[real]
    return np.concatenate([Eall, phi], 1) @ W1.T + b1


def make_scorer(model, std, p):
    """(t, members) -> s(c, t) for the members of the queue at t."""
    z = row_hidden_all(model, std, p)
    We = model.he.weight.numpy().astype(np.float64)
    W2 = model.out.weight.numpy().astype(np.float64)[0]
    b2 = float(model.out.bias.numpy()[0])
    kappa = float(model.kappa.numpy()[0])

    def score(t, members):
        e, lr = eng_feats(p, step_view(p, t), members)
        e = (e - std.mu_eng) / std.sd_eng
        return np.maximum(z[members] + e @ We.T, 0.0) @ W2 + b2 - kappa * lr
    return score


def gate_rows(model, std, npz_dir, names):
    per, rows = [], []
    for nm in names:
        path = os.path.join(npz_dir, nm + ".npz")
        if not os.path.exists(path):
            continue
        p = Problem(path)
        if not validate_levels(p):
            fix_levels(p)
        if p.label.sum() == 0:
            continue
        r = gate_problem(p, make_scorer(model, std, p))
        if r is None:
            continue
        med = {k: float(np.median(r["pct"][k])) for k in r["pct"]}
        hit = {k: float(np.mean(r["hit"][k])) for k in r["hit"]}
        per.append((nm, med, hit, len(r["hit"]["model"]), len(r["pct"]["model"]), p.rate,
                    int(p.label.sum()), p.n_sel))
        rows.append(r)
    return per, rows


def cmd_gate(model_prefix, npz_dir, pool_path, out_prefix, which="heldout"):
    model, std, meta = load_model(model_prefix)
    split = load_pool(pool_path)
    if which == "heldout":
        names = sorted(n for n, s in split.items() if s == "heldout")
    else:   # the validation slice carved from train (model selection only)
        names = sorted(n for n, s in split.items() if s == "train" and is_val(n))
    per, rows = gate_rows(model, std, npz_dir, names)
    with open(out_prefix + "_per_problem.tsv", "w") as f:
        f.write("problem\tsteps\tpairs\trate\tn_pos\tn_sel\tmed_model\tmed_engine\tmed_weight\t"
                "miss_model\tmiss_engine\tmiss_weight\n")
        for nm, med, hit, ns, npair, rate, npos, nsel in per:
            f.write(f"{nm}\t{ns}\t{npair}\t{rate:.4g}\t{npos}\t{nsel}\t{med['model']:.5f}\t"
                    f"{med['engine']:.5f}\t{med['weight']:.5f}\t{1 - hit['model']:.4f}\t"
                    f"{1 - hit['engine']:.4f}\t{1 - hit['weight']:.4f}\n")
    K = ["model", "engine", "weight"]
    meds = {k: np.array([x[1][k] for x in per]) for k in K}
    miss = {k: 1.0 - np.array([x[2][k] for x in per]) for k in K}
    lines = [f"{which} split, model {model_prefix} ({str(meta['variant'])}, best epoch "
             f"{int(meta['best_epoch'])}); eligible problems: {len(per)}"]
    for k in K:
        ci_m, _ = bootstrap(per, lambda idx, k=k: np.median(meds[k][idx]))
        ci_h, _ = bootstrap(per, lambda idx, k=k: np.mean(miss[k][idx]))
        allp = np.concatenate([np.array(r["pct"][k]) for r in rows])
        lines.append(f"{k:7s} median-of-problem-medians {np.median(meds[k]):.5f} "
                     f"CI [{ci_m[0]:.5f}, {ci_m[1]:.5f}]  pooled-pairs median "
                     f"{np.median(allp):.5f}  top10 miss {np.mean(miss[k]):.4f} "
                     f"CI [{ci_h[0]:.4f}, {ci_h[1]:.4f}] (hit {1 - np.mean(miss[k]):.4f})")
    r_med = np.median(meds["model"]) / max(1e-12, np.median(meds["engine"]))
    r_miss = np.mean(miss["model"]) / max(1e-12, np.mean(miss["engine"]))
    ci_rm, _ = bootstrap(per, lambda idx: np.median(meds["model"][idx]) /
                         max(1e-12, np.median(meds["engine"][idx])))
    ci_rh, _ = bootstrap(per, lambda idx: np.mean(miss["model"][idx]) /
                         max(1e-12, np.mean(miss["engine"][idx])))
    pass_m, pass_h = r_med <= 0.5, r_miss <= 0.5
    lines.append(f"ratio median model/engine {r_med:.4f} CI [{ci_rm[0]:.4f}, {ci_rm[1]:.4f}] "
                 f"(need <= 0.5: {'yes' if pass_m else 'no'}; CI clears: "
                 f"{'yes' if ci_rm[1] <= 0.5 else 'no'})")
    lines.append(f"ratio miss model/engine {r_miss:.4f} CI [{ci_rh[0]:.4f}, {ci_rh[1]:.4f}] "
                 f"(need <= 0.5: {'yes' if pass_h else 'no'}; CI clears: "
                 f"{'yes' if ci_rh[1] <= 0.5 else 'no'})")
    lines.append(f"selection score max(r_med, r_miss) {max(r_med, r_miss):.4f}")
    lines.append("GATE 0b: " + ("PASS" if (pass_m and pass_h) else "FAIL"))
    lines.append("secondary, by selections per problem (not the gate):")
    nsel = np.array([x[7] for x in per])
    for lo, hi in ((0, 100), (100, 1000), (1000, 10 ** 9)):
        m = (nsel >= lo) & (nsel < hi)
        if not m.any():
            continue
        parts = [f"  sel [{lo},{hi if hi < 10 ** 9 else 'inf'}) n={int(m.sum())}:"]
        for k in K:
            parts.append(f"{k} med {np.median(meds[k][m]):.4f} miss {np.mean(miss[k][m]):.3f}")
        lines.append(" ".join(parts))
    txt = "\n".join(lines)
    print(txt)
    with open(out_prefix + "_summary.txt", "w") as f:
        f.write(txt + "\n")


# --------------------------------------------------------------------------
# sanity: the segment loss matches a numpy reference, the batch rows embed
# like the inference layout, the untrained model ranks like the engine,
# and jitted Adam lowers the loss

def cmd_sanity(npz_dir, pool_path, n_probs=6, steps=30):
    split = load_pool(pool_path)
    names = sorted(n for n, s in split.items() if s == "train" and not is_val(n))
    probs = [p for p in load_problems(npz_dir, names[:40]) if len(candidate_steps(p))]
    probs = sorted(probs, key=lambda p: -p.n_core)[:n_probs]
    Tensor.manual_seed(0)
    rng = np.random.default_rng(0)
    std = Std(probs)
    std.mu_eng, std.sd_eng = eng_stats(probs, np.random.default_rng(1))
    ok = True
    for variant in ("H32", "N16"):
        dim, hid, history = VARIANTS[variant]
        model = ListNet(dim, hid, history)
        # a non-zero output layer, so the check covers the MLP path
        model.out.weight.assign(Tensor.uniform(1, hid, low=-0.5, high=0.5)).realize()
        fb = ListBatch(probs, std, dim, history)
        np_ins = fb.draw(rng)
        ins = [Tensor(x).realize() for x in np_ins]
        s = model.scores(fb, *ins[:6]).numpy()
        got = model.loss(fb, *ins).item()
        ref = list_loss_ref(s, np_ins[6], np_ins[7], np_ins[9], fb.off)
        # per-row embedding: the batch path against the gate's path
        T = model.table(fb.layout) if history else None
        Zb = model.rows(T, *ins[:4]).numpy()
        dz = 0.0
        for b, p in enumerate(probs):
            z = row_hidden_all(model, std, p)
            _, rows, _, _ = fb.last[b]
            c0 = int(fb.off[b])
            dz = max(dz, float(np.max(np.abs(Zb[c0:c0 + len(rows)] - z[rows]))))
        rel = abs(got - ref) / max(1e-9, abs(ref))
        print(f"{variant}: loss tinygrad {got:.6f} numpy {ref:.6f} (rel {rel:.1e}); batch vs "
              f"gate row pre-activation max |diff| {dz:.1e}")
        ok &= rel < 1e-4 and dz < 1e-3
    # untrained (W2 = 0): the model's gate numbers equal the engine's
    # wherever the rank estimate is exact (rate 1, every positive sampled)
    model = ListNet(16, 16, True)
    for p in probs:
        r = gate_problem(p, make_scorer(model, std, p))
        eq = (np.allclose(r["pct"]["model"], r["pct"]["engine"])
              and r["hit"]["model"] == r["hit"]["engine"])
        exact = p.rate == 1.0 and bool(np.all(p.in_sample[p.label]))
        print(f"  {p.name}: rate {p.rate:.3g} exact {exact} untrained model median "
              f"{np.median(r['pct']['model']):.4f} engine {np.median(r['pct']['engine']):.4f} "
              f"{'identical' if eq else 'DIFFERENT'}")
        ok &= eq or not exact
    fb = ListBatch(probs, std, 16, True)
    params = get_parameters(model)
    opt = nn.optim.Adam(params, lr=3e-3)
    fixed = fb.sample(rng)
    # a forward pass first realizes the parameters (as the training loop's
    # validation pass does), then Adam's first step moves each by <= lr
    model.loss(fb, *fixed).realize()
    before = [x.numpy().copy() for x in params]

    def step(*ins):
        loss = model.loss(fb, *ins)
        opt.zero_grad(); loss.backward(); opt.step()
        return loss.realize()
    jit = TinyJit(step)
    losses, times, first = [], [], None
    with train_mode():
        for it in range(steps):
            t0 = time.time()
            losses.append(jit(*fixed).item())
            times.append(time.time() - t0)
            if it == 0:
                first = max(float(np.max(np.abs(x.numpy() - b0))) for x, b0 in zip(params, before))
    moved = max(float(np.max(np.abs(x.numpy() - b0))) for x, b0 in zip(params, before))
    finite = all(np.all(np.isfinite(x.numpy())) for x in params)
    print(f"loss {losses[0]:.4f} -> {losses[-1]:.4f} over {steps} jitted Adam steps; first "
          f"step max |param delta| {first:.3e} (lr 3e-3); after {steps} {moved:.3e}; params "
          f"finite {finite}; step s: first {times[0]:.1f} second {times[1]:.1f} replay "
          f"{np.median(times[2:]):.3f}")
    ok &= losses[-1] < 0.9 * losses[0] and 0 < first <= 3.01e-3 and finite
    print("SANITY " + ("OK" if ok else "FAIL"))


def main():
    cmd = sys.argv[1]
    if cmd == "train":
        cmd_train(*sys.argv[2:6])
    elif cmd == "gate":
        cmd_gate(*sys.argv[2:6])
    elif cmd == "valgate":
        cmd_gate(*sys.argv[2:6], which="val")
    elif cmd == "sanity":
        cmd_sanity(sys.argv[2], sys.argv[3])


if __name__ == "__main__":
    main()
