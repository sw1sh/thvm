#!/usr/bin/env python3
# Neural clause selection, phase 0d: a SELECTION-TIME re-ranker over the
# engine's top K, plus the pre-registered phase-0d gate
# (~/.cache/thvm/casc/J13/neural_p2/gate_spec_0d.txt).
#
# At a weight (non-FIFO) selection t the candidates are the engine's top
# K of the queue under its own key (cp_pri at t, seq), with engine rank
# r = 0..K-1.  The head scores each from what a selector has then:
#   x(c, t) = [log1p(r), log1p(queue size), log1p(cp_pri at t),
#              f0..f13 (the 14 push features, 'F' records),
#              log1p(t - push step)]
#   s(c, t) = -kappa log1p(r) + W2 relu(W1 z + b1) + b2,  z = (x - mu) / sd
# and the re-ranker selects the argmax.  W2 and b2 start at zero and kappa
# at 1, so the untrained head selects r = 0, the engine's own pick.
# Training is listwise within the top K: at a step, -log of the softmax
# mass of the pending proof pairs among the K candidates.
#
# The queue is rebuilt from the history export exactly as phases 0-0c
# (tools/atp_enigma/history_net.py); only exact (rate-1) problems take
# part, since a subsampled problem's top K would miss unsampled CPs.
#
# Usage:
#   topk_net.py prep    <npz_dir> <pool.tsv> <prep_dir> <precheck.txt> [jobs]
#   topk_net.py sanity  <prep_dir> <pool.tsv>
#   topk_net.py train   <prep_dir> <pool.tsv> <out_prefix> <variant>
#   topk_net.py valgate <model_prefix> <prep_dir> <pool.tsv> <out_prefix>
#   topk_net.py gate    <model_prefix> <prep_dir> <pool.tsv> <out_prefix>
#   topk_net.py select  <out.tsv> <ratios.tsv>...
import heapq
import os
import sys
import time
from multiprocessing import Pool

import numpy as np
from tinygrad import Tensor, TinyJit, nn
from tinygrad.nn.state import get_parameters, get_state_dict, load_state_dict, safe_load, safe_save

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from history_net import bootstrap, candidate_steps, engine_pri_at, is_val, load_pool, train_mode

KMAX = 32
N_CPF = 14
N_X = 3 + N_CPF + 1
S_STEPS = 16
BATCH = 16
GATE_STEPS = 50
POOL_STEPS = 400
BIG = 1e4
VARIANTS = {f"R{k}h{h}": (k, h) for k in (8, 16, 32) for h in (16, 32)}
KS = (8, 16, 32)
# selection classes
C_WEIGHT, C_WEIGHT_ULT, C_FIFO, C_ULT, C_OTHER, C_MISSING = range(6)
C_NAMES = ("weight", "weight_ultimate_alive", "fifo", "ultimate", "other", "pick_missing")


# --------------------------------------------------------------------------
# one problem's queue history (no derivation DAG)

class Run:
    def __init__(self, path):
        d = np.load(path)
        self.name = os.path.basename(path)[:-4]
        meta = d["meta"]
        self.rate = float(meta[0])
        self.n_sel = int(meta[2])
        self.seq = d["seq"].astype(np.int64)
        self.label = d["label"].astype(bool)
        self.in_sample = d["in_sample"].astype(bool)
        self.has_push = d["has_push"].astype(bool)
        self.selected_at = d["selected_at"].astype(np.int64)
        self.push_step = d["push_step"].astype(np.int64)
        self.end_step = d["end_step"].astype(np.int64)
        self.pri = np.where(d["has_push"], d["pri_push"], d["pri"]).astype(np.int64)
        self.ult = (d["flags"].astype(np.int64) & 1) != 0
        self.cpfeat = d["cpfeat"].astype(np.float64) if "cpfeat" in d else None
        self.has_cpfeat = d["has_cpfeat"].astype(bool) if "has_cpfeat" in d else None
        self.w_seq = d["w_seq"].astype(np.int64)
        self.w_step = d["w_step"].astype(np.int64)
        self.w_pri = d["w_pri"].astype(np.int64)
        self.x_step = d["x_step"].astype(np.int64)
        self.x_live = d["x_live"].astype(np.int64)


def ineligible(p):
    """The reason p takes no part in phase 0d, or None."""
    if p.label.sum() == 0:
        return "no_positive"
    if p.n_sel < 2:
        return "few_selections"
    if p.cpfeat is None or not bool(np.all(p.has_cpfeat)):
        return "no_push_features"
    if p.rate < 1.0 or not bool(np.all(p.in_sample == p.has_push)):
        return "subsampled"
    return None


def classify(p):
    """Class of every selection t = 1..n_sel (index 0 unused) and its
    pick, by one sweep over the push / death / reweight events with lazy
    min-heaps under the engine key (non-ultimate CPs) and under seq."""
    T, N = p.n_sel, len(p.seq)
    cls = np.full(T + 1, C_MISSING, np.int8)
    sel = p.selected_at
    ok = (sel >= 1) & (sel <= T)
    cnt = np.bincount(sel[ok], minlength=T + 1)
    pick = np.full(T + 1, -1, np.int64)
    pick[sel[ok]] = np.nonzero(ok)[0]
    pick[cnt != 1] = -1
    t_on = np.where(p.end_step >= p.push_step + 1, p.push_step + 1, T + 2)
    t_off = np.minimum(p.end_step + 1, T + 2)
    on = np.argsort(t_on, kind="stable").tolist()
    off = np.argsort(t_off, kind="stable").tolist()
    t_on_l, t_off_l = t_on.tolist(), t_off.tolist()
    wi = np.searchsorted(p.seq, p.w_seq)
    wok = (wi < N) & (p.seq[np.minimum(wi, max(N - 1, 0))] == p.w_seq) if N else wi < 0
    w_t = (p.w_step + 1).tolist()
    w_i = np.where(wok, wi, -1).tolist()
    w_p = p.w_pri.tolist()
    worder = np.argsort(p.w_step + 1, kind="stable").tolist()
    cur = p.pri.tolist()
    seq = p.seq.tolist()
    ult = p.ult.tolist()
    ver = [0] * N
    alive = [False] * N
    wh, fh = [], []
    n_ult = 0
    ion = ioff = iw = 0
    nw = len(worder)
    for t in range(1, T + 1):
        while ion < N and t_on_l[on[ion]] <= t:
            i = on[ion]
            ion += 1
            alive[i] = True
            if ult[i]:
                n_ult += 1
            else:
                heapq.heappush(wh, (cur[i], seq[i], ver[i], i))
                heapq.heappush(fh, (seq[i], i))
        while ioff < N and t_off_l[off[ioff]] <= t:
            i = off[ioff]
            ioff += 1
            if alive[i]:
                alive[i] = False
                if ult[i]:
                    n_ult -= 1
        while iw < nw and w_t[worder[iw]] <= t:
            k = worder[iw]
            iw += 1
            i = w_i[k]
            if i < 0:
                continue
            cur[i] = w_p[k]
            ver[i] += 1
            if alive[i] and not ult[i]:
                heapq.heappush(wh, (cur[i], seq[i], ver[i], i))
        j = int(pick[t])
        if j < 0:
            continue
        if ult[j]:
            cls[t] = C_ULT
            continue
        while wh and (not alive[wh[0][3]] or wh[0][2] != ver[wh[0][3]]):
            heapq.heappop(wh)
        while fh and not alive[fh[0][1]]:
            heapq.heappop(fh)
        if not alive[j]:
            cls[t] = C_OTHER
        elif wh and wh[0][3] == j:
            cls[t] = C_WEIGHT if n_ult == 0 else C_WEIGHT_ULT
        elif fh and fh[0][1] == j:
            cls[t] = C_FIFO
        else:
            cls[t] = C_OTHER
    return cls, pick


def spaced(steps, n):
    """At most n of steps, evenly spaced (the 0b / 0c step rule)."""
    if len(steps) == 0:
        return steps
    k = min(n, len(steps))
    return np.unique(steps[np.round(np.linspace(0, len(steps) - 1, k)).astype(int)])


def top_at(p, t, pri_t, k=KMAX):
    """The engine's top k at selection t (kept-table indices in key
    order) and the queue size."""
    alive = (p.push_step <= t - 1) & (p.end_step >= t)
    idx = np.nonzero(alive)[0]
    key = (pri_t[idx].astype(np.uint64) << np.uint64(32)) | p.seq[idx].astype(np.uint64)
    part = np.argpartition(key, k - 1)[:k] if len(idx) > k else np.arange(len(idx))
    part = part[np.argsort(key[part], kind="stable")]
    return idx[part], len(idx)


def step_x(p, t, top, n_alive, pri_t):
    """Raw inputs of the candidates top at selection t (KMAX rows, zero
    padded)."""
    x = np.zeros((KMAX, N_X), np.float32)
    k = len(top)
    x[:k, 0] = np.log1p(np.arange(k))
    x[:k, 1] = np.log1p(n_alive)
    x[:k, 2] = np.log1p(pri_t[top].astype(np.float64))
    x[:k, 3:3 + N_CPF] = p.cpfeat[top]
    x[:k, 3 + N_CPF] = np.log1p(t - p.push_step[top])
    return x


def prep_one(args):
    """Classify, pick the gate and pool steps, and store their top KMAX
    candidates with inputs and labels."""
    path, split, out_dir = args
    nm = os.path.basename(path)[:-4]
    row = {"name": nm, "split": split, "reason": "", "n_sel": 0, "cls": [0] * 6,
           "n_rw": 0, "p4_n": 0, "p4_ok": 0, "rank0_bad": 0, "n_c": 0, "n_cp": 0}
    if not os.path.exists(path):
        row["reason"] = "no_npz"
        return row
    p = Run(path)
    row["n_sel"] = p.n_sel
    row["n_rw"] = len(p.w_seq)
    why = ineligible(p)
    if why is not None:
        row["reason"] = why
        return row
    cls, pick = classify(p)
    row["cls"] = np.bincount(cls[1:], minlength=6).tolist()
    cand = candidate_steps(p)
    cprime = cand[cls[cand] == C_WEIGHT]
    row["n_c"], row["n_cp"] = len(cand), len(cprime)
    if len(cprime) == 0:
        row["reason"] = "no_gate_step"
        return row
    g = spaced(cprime, GATE_STEPS)
    q = spaced(cprime, POOL_STEPS)
    steps = np.union1d(g, q)
    n = len(steps)
    top = np.full((n, KMAX), -1, np.int32)
    lab = np.zeros((n, KMAX), bool)
    X = np.zeros((n, KMAX, N_X), np.float32)
    n_alive = np.zeros(n, np.int64)
    xs = np.argsort(p.x_step, kind="stable")
    for i, t in enumerate(steps.tolist()):
        pri_t = engine_pri_at(p, t) if len(p.w_seq) else p.pri
        tp, na = top_at(p, t, pri_t)
        top[i, :len(tp)] = tp
        lab[i, :len(tp)] = p.label[tp]
        X[i] = step_x(p, t, tp, na, pri_t)
        n_alive[i] = na
        if len(tp) == 0 or tp[0] != pick[t]:
            row["rank0_bad"] += 1
        if np.any(g == t):
            k = np.searchsorted(p.x_step[xs], t)
            if k < len(xs) and p.x_step[xs[k]] == t:
                row["p4_n"] += 1
                row["p4_ok"] += int(na == p.x_live[xs[k]] + 1)
    np.savez_compressed(os.path.join(out_dir, nm + ".npz"), steps=steps,
                        is_gate=np.isin(steps, g), is_pool=np.isin(steps, q), top=top,
                        lab=lab, X=X, n_alive=n_alive, n_sel=p.n_sel, n_rw=len(p.w_seq))
    row["reason"] = "ok"
    return row


def cmd_prep(npz_dir, pool_path, prep_dir, out_path, jobs=8):
    os.makedirs(prep_dir, exist_ok=True)
    split = load_pool(pool_path)
    names = sorted(split)
    t0 = time.time()
    args = [(os.path.join(npz_dir, nm + ".npz"), split[nm], prep_dir) for nm in names]
    rows = []
    with Pool(int(jobs)) as pool:
        for r in pool.imap_unordered(prep_one, args, chunksize=1):
            rows.append(r)
    rows.sort(key=lambda r: r["name"])
    with open(os.path.join(prep_dir, "prep.tsv"), "w") as f:
        f.write("name\tsplit\tpart\treason\tn_sel\tn_reweights\tn_cand\tn_weight_cand\t"
                + "\t".join(C_NAMES) + "\tp4_n\tp4_ok\trank0_bad\n")
        for r in rows:
            part = part_of(r["name"], r["split"])
            f.write(f"{r['name']}\t{r['split']}\t{part}\t{r['reason']}\t{r['n_sel']}\t{r['n_rw']}\t"
                    f"{r['n_c']}\t{r['n_cp']}\t" + "\t".join(str(c) for c in r["cls"])
                    + f"\t{r['p4_n']}\t{r['p4_ok']}\t{r['rank0_bad']}\n")
    lines = [f"phase-0d prep over {len(rows)} pool problems ({time.time() - t0:.0f}s)"]
    for part in ("train", "val", "heldout"):
        rr = [r for r in rows if part_of(r["name"], r["split"]) == part and r["reason"] != "no_npz"]
        reasons = {}
        for r in rr:
            reasons[r["reason"]] = reasons.get(r["reason"], 0) + 1
        lines.append(f"{part}: with npz {len(rr)}; " + " ".join(f"{k}={v}" for k, v in
                                                               sorted(reasons.items())))
    ok = [r for r in rows if r["reason"] in ("ok", "no_gate_step")]
    tot = np.sum([r["cls"] for r in ok], axis=0) if ok else np.zeros(6, int)
    lines.append("P1 selections of the eligible problems by class: "
                 + " ".join(f"{n}={int(c)}" for n, c in zip(C_NAMES, tot)))
    rb = sum(r["rank0_bad"] for r in ok)
    lines.append(f"P1 stored steps whose rebuilt top-K rank 0 is not the pick: {rb}")
    rw = [r for r in ok if r["n_rw"] > 0]
    lines.append(f"P3 eligible problems with in-place reweights: {len(rw)} (W rows "
                 f"{sum(r['n_rw'] for r in rw)}); by part: "
                 + " ".join(f"{pt}={sum(1 for r in rw if part_of(r['name'], r['split']) == pt)}"
                            for pt in ("train", "val", "heldout")))
    p4n = sum(r["p4_n"] for r in ok)
    p4k = sum(r["p4_ok"] for r in ok)
    lines.append(f"P4 gate steps with an 'X' record: {p4n}; queue size == X live + 1: {p4k} "
                 f"({p4k / max(1, p4n):.4f})")
    stop = int(tot[C_OTHER]) + int(tot[C_MISSING]) + rb
    lines.append("PRECHECK " + ("OK" if stop == 0 else "STOP"))
    txt = "\n".join(lines)
    print(txt)
    with open(out_path, "w") as f:
        f.write(txt + "\n")


def part_of(name, split):
    if split == "heldout":
        return "heldout"
    return "val" if is_val(name) else "train"


# --------------------------------------------------------------------------
# prepared problems

class Prep:
    def __init__(self, path):
        d = np.load(path)
        self.name = os.path.basename(path)[:-4]
        self.steps = d["steps"]
        self.is_gate = d["is_gate"]
        self.is_pool = d["is_pool"]
        self.top = d["top"]
        self.lab = d["lab"]
        self.X = d["X"]
        self.n_alive = d["n_alive"]
        self.n_sel = int(d["n_sel"])


def load_part(prep_dir, pool_path, part):
    split = load_pool(pool_path)
    out = []
    for nm in sorted(split):
        if part_of(nm, split[nm]) != part:
            continue
        path = os.path.join(prep_dir, nm + ".npz")
        if os.path.exists(path):
            out.append(Prep(path))
    return out


def pool_arrays(pp, k, mu=None, sd=None):
    """Training-pool arrays of problem pp for top k: raw or standardized
    inputs, log1p(r), member / positive log-masks, usable steps."""
    sel = np.nonzero(pp.is_pool)[0]
    X = pp.X[sel, :k].astype(np.float64)
    mem = pp.top[sel, :k] >= 0
    pos = pp.lab[sel, :k] & mem
    if mu is not None:
        X = (X - mu) / sd
    lr = np.log1p(np.arange(k, dtype=np.float64))[None, :].repeat(len(sel), 0)
    lw = np.where(mem, 0.0, -BIG)
    pm = np.where(pos, 0.0, -BIG)
    usable = np.nonzero(pos.any(1))[0]
    return X, lr, lw, pm, usable, mem


def fit_std(probs, k):
    xs = []
    for pp in probs:
        X, _, _, _, _, mem = pool_arrays(pp, k)
        xs.append(X[mem])
    x = np.concatenate(xs)
    return x.mean(0), x.std(0) + 1e-6


class TopKNet:
    def __init__(self, hid):
        self.hid = hid
        self.h1 = nn.Linear(N_X, hid)
        self.out = nn.Linear(hid, 1)
        # numpy-backed: tinygrad 0.11 hash-conses Tensor.zeros(...).contiguous(),
        # so a parameter built that way shares its buffer with Adam's
        # same-shaped state once a forward pass realizes it
        self.out.weight = Tensor(np.zeros((1, hid), np.float32))
        self.out.bias = Tensor(np.zeros(1, np.float32))
        self.kappa = Tensor(np.ones(1, np.float32))

    def scores(self, z, lr):
        B, S, K, _ = z.shape
        return self.out(self.h1(z).relu()).reshape(B, S, K) - self.kappa * lr

    def loss(self, z, lr, lw, pm, sw):
        return topk_loss(self.scores(z, lr), lw, pm, sw)


def topk_loss(s, lw, pm, sw):
    """sum over (problem b, step slot j) of sw[b, j] (lse over the top K
    - lse over its positives), each shifted by its max (no gradient
    through the shift)."""
    B, S = sw.shape
    X = s + lw
    Xp = s + pm
    m = X.max(axis=2, keepdim=True).detach()
    mp = Xp.max(axis=2, keepdim=True).detach()
    den = (X - m).exp().sum(axis=2).log() + m.reshape(B, S)
    num = (Xp - mp).exp().sum(axis=2).log() + mp.reshape(B, S)
    return (sw * (den - num)).sum()


def topk_loss_ref(s, lw, pm, sw):
    s = s.astype(np.float64)
    tot = 0.0
    B, S = sw.shape
    for b in range(B):
        for j in range(S):
            if sw[b, j] == 0:
                continue
            x = s[b, j]
            wa = (lw[b, j] > -BIG / 2).astype(np.float64)
            wp = (pm[b, j] > -BIG / 2).astype(np.float64)
            mx = x.max()
            tot += sw[b, j] * (np.log(np.sum(wa * np.exp(x - mx))) - np.log(np.sum(wp * np.exp(x - mx))))
    return tot


class Batcher:
    """Fixed-shape [BATCH, S_STEPS, k] draws over a problem list."""

    def __init__(self, probs, k, mu, sd):
        self.k = k
        self.arr = []
        for pp in probs:
            X, lr, lw, pm, usable, _ = pool_arrays(pp, k, mu, sd)
            if len(usable):
                self.arr.append((X.astype(np.float32), lr.astype(np.float32),
                                 lw.astype(np.float32), pm.astype(np.float32), usable))
        self.names = [pp.name for pp in probs]

    def draw(self, idx, rng):
        k, S = self.k, S_STEPS
        Z = np.zeros((BATCH, S, k, N_X), np.float32)
        LR = np.zeros((BATCH, S, k), np.float32)
        LW = np.full((BATCH, S, k), -BIG, np.float32)
        PM = np.full((BATCH, S, k), -BIG, np.float32)
        SW = np.zeros((BATCH, S), np.float32)
        for b, i in enumerate(idx):
            X, lr, lw, pm, usable = self.arr[i]
            st = np.sort(rng.choice(usable, S, replace=False)) if len(usable) > S else usable
            n = len(st)
            Z[b, :n], LR[b, :n], LW[b, :n], PM[b, :n] = X[st], lr[st], lw[st], pm[st]
            SW[b, :n] = 1.0 / (len(idx) * n)
        return Z, LR, LW, PM, SW


def tensors(xs):
    return [Tensor(x).realize() for x in xs]


def save_model(prefix, model, mu, sd, meta):
    safe_save(get_state_dict(model), prefix + ".safetensors")
    np.savez(prefix + "_std.npz", mu=mu, sd=sd, **meta)


def load_model(prefix):
    m = dict(np.load(prefix + "_std.npz"))
    model = TopKNet(int(m["hid"]))
    load_state_dict(model, safe_load(prefix + ".safetensors"))
    return model, m


# --------------------------------------------------------------------------
# training

def cmd_train(prep_dir, pool_path, out_prefix, variant, epochs=30, patience=5, lr=1e-3, seed=0):
    k, hid = VARIANTS[variant]
    t0 = time.time()
    tr = load_part(prep_dir, pool_path, "train")
    va = load_part(prep_dir, pool_path, "val")
    mu, sd = fit_std(tr, k)
    tb = Batcher(tr, k, mu, sd)
    vb = Batcher(va, k, mu, sd)
    Tensor.manual_seed(seed)
    rng = np.random.default_rng(seed)
    model = TopKNet(hid)
    opt = nn.optim.Adam(get_parameters(model), lr=lr)
    print(f"{variant}: train {len(tb.arr)} of {len(tr)} problems with a usable step, val "
          f"{len(vb.arr)} of {len(va)}; loaded in {time.time() - t0:.0f}s; input mu "
          f"{np.round(mu, 3).tolist()} sd {np.round(sd, 3).tolist()}", flush=True)

    def step(*ins):
        loss = model.loss(*ins)
        opt.zero_grad()
        loss.backward()
        opt.step()
        return loss.realize()
    jit = TinyJit(step)
    vjit = TinyJit(lambda *ins: model.loss(*ins).realize())
    vrng = np.random.default_rng(1234)
    vfix = []
    for b0 in range(0, len(vb.arr), BATCH):
        idx = list(range(b0, min(b0 + BATCH, len(vb.arr))))
        vfix.append((len(idx), tensors(vb.draw(idx, vrng))))
    nva = len(vb.arr)
    meta = dict(k=k, hid=hid, variant=variant, seed=seed, lr=lr, S=S_STEPS, batch=BATCH)

    def val_loss():
        return sum(vjit(*ins).item() * n for n, ins in vfix) / nva
    vl0 = val_loss()
    print(f"epoch -1 (the engine's rank alone, kappa 1) val {vl0:.4f}", flush=True)
    best = (np.inf, -1)
    for ep in range(epochs):
        te = time.time()
        tl = 0.0
        perm = rng.permutation(len(tb.arr))
        with train_mode():
            for b0 in range(0, len(perm), BATCH):
                idx = perm[b0:b0 + BATCH].tolist()
                tl += jit(*tensors(tb.draw(idx, rng))).item() * len(idx)
        vl = val_loss()
        print(f"epoch {ep} train {tl / len(tb.arr):.4f} val {vl:.4f} kappa "
              f"{model.kappa.numpy()[0]:.3f} ({time.time() - te:.0f}s)", flush=True)
        if vl < best[0]:
            best = (vl, ep)
            save_model(out_prefix, model, mu, sd, dict(meta, best_epoch=ep, best_val=vl, val0=vl0))
        elif ep - best[1] >= patience:
            break
    print(f"saved {out_prefix} best epoch {best[1]} val {best[0]:.4f}")


# --------------------------------------------------------------------------
# gate (gate_spec_0d.txt): numpy float64 from the saved weights

class Head:
    def __init__(self, model, mu, sd):
        self.W1 = model.h1.weight.numpy().astype(np.float64)
        self.b1 = model.h1.bias.numpy().astype(np.float64)
        self.W2 = model.out.weight.numpy().astype(np.float64)[0]
        self.b2 = float(model.out.bias.numpy()[0])
        self.kappa = float(model.kappa.numpy()[0])
        self.mu, self.sd = mu.astype(np.float64), sd.astype(np.float64)

    def __call__(self, X):
        z = (X.astype(np.float64) - self.mu) / self.sd
        r = np.arange(len(X), dtype=np.float64)
        return np.maximum(z @ self.W1.T + self.b1, 0.0) @ self.W2 + self.b2 - self.kappa * np.log1p(r)


def gate_problem(pp, k, head):
    """Per gate step: engine / re-ranker hit, positive in the top k, and
    the best-positive rank under each ranker (-1 when none)."""
    out = {"hE": [], "hR": [], "o": [], "bE": [], "bR": []}
    for i in np.nonzero(pp.is_gate)[0]:
        top = pp.top[i, :k]
        n = int(np.sum(top >= 0))
        lab = pp.lab[i, :n]
        s = head(pp.X[i, :n]) if head is not None else -np.log1p(np.arange(n))
        a = int(np.argmax(s))
        out["hE"].append(float(lab[0]))
        out["hR"].append(float(lab[a]))
        out["o"].append(float(lab.any()))
        if lab.any():
            order = np.argsort(-s, kind="stable")
            out["bE"].append(int(np.argmax(lab)))
            out["bR"].append(int(np.argmax(lab[order])))
    return out


RANK_BINS = ((0, 0), (1, 1), (2, 3), (4, 7), (8, 15), (16, 31))


def rank_hist(vals):
    v = np.array(vals)
    return " ".join(f"{a}{'' if a == b else '-' + str(b)}:{np.mean((v >= a) & (v <= b)):.3f}"
                    for a, b in RANK_BINS) if len(v) else "-"


def gate_stats(per):
    """Pooled statistics over problems (per: list of (name, n_sel, out))."""
    hE = np.array([np.mean(o["hE"]) for _, _, o in per])
    hR = np.array([np.mean(o["hR"]) for _, _, o in per])
    oo = np.array([np.mean(o["o"]) for _, _, o in per])
    return hE, hR, oo


def cmd_gate(model_prefix, prep_dir, pool_path, out_prefix, which="heldout"):
    model, m = load_model(model_prefix)
    k = int(m["k"])
    head = Head(model, m["mu"], m["sd"])
    probs = load_part(prep_dir, pool_path, "heldout" if which == "heldout" else "val")
    probs = [pp for pp in probs if pp.is_gate.any()]
    per = [(pp.name, pp.n_sel, gate_problem(pp, k, head)) for pp in probs]
    hE, hR, oo = gate_stats(per)
    rho = hR.mean() / max(1e-12, hE.mean())
    ci_r, _ = bootstrap(per, lambda idx: hR[idx].mean() / max(1e-12, hE[idx].mean()))
    ci_e, _ = bootstrap(per, lambda idx: hE[idx].mean())
    ci_m, _ = bootstrap(per, lambda idx: hR[idx].mean())
    ci_o, _ = bootstrap(per, lambda idx: oo[idx].mean())
    better = int(np.sum(hR > hE))
    worse = int(np.sum(hR < hE))
    bE = [b for _, _, o in per for b in o["bE"]]
    bR = [b for _, _, o in per for b in o["bR"]]
    mbE = np.mean([np.mean(o["bE"]) for _, _, o in per if o["bE"]])
    mbR = np.mean([np.mean(o["bR"]) for _, _, o in per if o["bR"]])
    lines = [f"{which} split, model {model_prefix} ({str(m['variant'])}, K {k}, hidden "
             f"{int(m['hid'])}, best epoch {int(m['best_epoch'])}); problems {len(per)}, gate steps "
             f"{sum(len(o['hE']) for _, _, o in per)}",
             f"engine    top-1 hit H_E {hE.mean():.4f} CI [{ci_e[0]:.4f}, {ci_e[1]:.4f}]",
             f"re-ranker top-1 hit H_R {hR.mean():.4f} CI [{ci_m[0]:.4f}, {ci_m[1]:.4f}]",
             f"ratio rho = H_R / H_E {rho:.4f} CI [{ci_r[0]:.4f}, {ci_r[1]:.4f}] "
             f"(lower end > 1: {'yes' if ci_r[0] > 1.0 else 'no'})",
             f"per problem: better {better} / worse {worse} / tie {len(per) - better - worse}",
             f"ceiling K {k}: top K holds a positive H_O {oo.mean():.4f} CI [{ci_o[0]:.4f}, "
             f"{ci_o[1]:.4f}]; none {1 - oo.mean():.4f}; H_O / H_E {oo.mean() / max(1e-12, hE.mean()):.4f}",
             f"best-positive rank (steps with a positive in the top K): engine mean-of-problem-means "
             f"{mbE:.3f} [{rank_hist(bE)}]; re-ranker {mbR:.3f} [{rank_hist(bR)}]"]
    lines.append("model-free, every K (engine pick r = 0; ceiling = a positive in the top K):")
    for kk in KS:
        pk = [(nm, ns, gate_problem(pp, kk, None)) for (nm, ns, _), pp in zip(per, probs)]
        e2, _, o2 = gate_stats(pk)
        ci2, _ = bootstrap(pk, lambda idx: o2[idx].mean())
        b2 = [b for _, _, o in pk for b in o["bE"]]
        lines.append(f"  K {kk:2d}: H_E {e2.mean():.4f}  H_O {o2.mean():.4f} CI [{ci2[0]:.4f}, "
                     f"{ci2[1]:.4f}]  none {1 - o2.mean():.4f}  H_O/H_E "
                     f"{o2.mean() / max(1e-12, e2.mean()):.4f}  engine best-positive rank "
                     f"[{rank_hist(b2)}]")
    lines.append("GATE 0d: " + ("PASS" if ci_r[0] > 1.0 else "FAIL"))
    lines.append("secondary, by selections per problem (not the gate):")
    nsel = np.array([ns for _, ns, _ in per])
    for lo, hi in ((0, 100), (100, 1000), (1000, 10 ** 9)):
        mk = (nsel >= lo) & (nsel < hi)
        if not mk.any():
            continue
        lines.append(f"  sel [{lo},{hi if hi < 10 ** 9 else 'inf'}) n={int(mk.sum())}: H_E "
                     f"{hE[mk].mean():.4f} H_R {hR[mk].mean():.4f} H_O {oo[mk].mean():.4f} "
                     f"better {int(np.sum(hR[mk] > hE[mk]))} worse {int(np.sum(hR[mk] < hE[mk]))}")
    txt = "\n".join(lines)
    print(txt)
    with open(out_prefix + "_summary.txt", "w") as f:
        f.write(txt + "\n")
    with open(out_prefix + "_per_problem.tsv", "w") as f:
        f.write("problem\tn_sel\tgate_steps\th_engine\th_rerank\ttopk_has_pos\n")
        for (nm, ns, o), e, r, c in zip(per, hE, hR, oo):
            f.write(f"{nm}\t{ns}\t{len(o['hE'])}\t{e:.4f}\t{r:.4f}\t{c:.4f}\n")
    with open(out_prefix + "_ratios.tsv", "w") as f:
        f.write("variant\tprefix\tk\thid\tbest_epoch\tbest_val\trho\trho_lo\trho_hi\n")
        f.write(f"{str(m['variant'])}\t{model_prefix}\t{k}\t{int(m['hid'])}\t{int(m['best_epoch'])}\t"
                f"{float(m['best_val']):.4f}\t{rho:.6f}\t{ci_r[0]:.6f}\t{ci_r[1]:.6f}\n")


def cmd_select(out_path, *ratio_paths):
    """The fixed rule: highest validation rho; ties to the smaller K, then
    the smaller hidden width."""
    rows = []
    for rp in ratio_paths:
        with open(rp) as f:
            next(f)
            v, pre, k, hid, ep, bv, rho, lo, hi = next(f).rstrip("\n").split("\t")
            rows.append((-float(rho), int(k), int(hid), v, pre, ep, bv, rho, lo, hi))
    rows.sort()
    with open(out_path, "w") as f:
        f.write("variant\tprefix\tk\thid\tbest_epoch\tbest_val\trho\trho_lo\trho_hi\tstatus\n")
        for i, (_, k, hid, v, pre, ep, bv, rho, lo, hi) in enumerate(rows):
            f.write(f"{v}\t{pre}\t{k}\t{hid}\t{ep}\t{bv}\t{rho}\t{lo}\t{hi}\t"
                    f"{'CHOSEN' if i == 0 else ''}\n")
    print(open(out_path).read().rstrip())


# --------------------------------------------------------------------------
# sanity: the loss matches a numpy reference, the untrained head
# reproduces the engine (P2), and jitted Adam lowers the loss

def cmd_sanity(prep_dir, pool_path, steps=30):
    ok = True
    va = [pp for pp in load_part(prep_dir, pool_path, "val") if pp.is_gate.any()]
    bad = 0
    for kk in KS:
        for pp in va:
            o = gate_problem(pp, kk, Head(TopKNet(16), np.zeros(N_X), np.ones(N_X)))
            if o["hE"] != o["hR"]:
                bad += 1
    print(f"P2 untrained head vs engine on {len(va)} validation problems x {len(KS)} K: "
          f"{bad} differ")
    ok &= bad == 0
    tr = load_part(prep_dir, pool_path, "train")[:48]
    k = 16
    mu, sd = fit_std(tr, k)
    bt = Batcher(tr, k, mu, sd)
    Tensor.manual_seed(0)
    rng = np.random.default_rng(0)
    model = TopKNet(16)
    model.out.weight.assign(Tensor.uniform(1, 16, low=-0.5, high=0.5)).realize()
    np_ins = bt.draw(list(range(min(BATCH, len(bt.arr)))), rng)
    ins = tensors(np_ins)
    s = model.scores(ins[0], ins[1]).numpy()
    got = model.loss(*ins).item()
    ref = topk_loss_ref(s, np_ins[2], np_ins[3], np_ins[4])
    rel = abs(got - ref) / max(1e-9, abs(ref))
    head = Head(model, mu, sd)
    ds = 0.0
    for b in range(min(BATCH, len(bt.arr))):
        for j in range(S_STEPS):
            if np_ins[4][b, j] == 0:
                continue
            mem = np_ins[2][b, j] > -BIG / 2
            Xraw = np_ins[0][b, j][mem].astype(np.float64) * sd + mu
            ds = max(ds, float(np.max(np.abs(head(Xraw) - s[b, j][mem]))))
    print(f"loss tinygrad {got:.6f} numpy {ref:.6f} (rel {rel:.1e}); batch vs gate score max "
          f"|diff| {ds:.1e}")
    ok &= rel < 1e-4 and ds < 1e-3
    model = TopKNet(16)
    params = get_parameters(model)
    opt = nn.optim.Adam(params, lr=3e-3)
    fixed = tensors(bt.draw(list(range(min(BATCH, len(bt.arr)))), rng))
    model.loss(*fixed).realize()
    before = [x.numpy().copy() for x in params]

    def step(*a):
        loss = model.loss(*a)
        opt.zero_grad()
        loss.backward()
        opt.step()
        return loss.realize()
    jit = TinyJit(step)
    losses, first = [], None
    with train_mode():
        for it in range(steps):
            losses.append(jit(*fixed).item())
            if it == 0:
                first = max(float(np.max(np.abs(x.numpy() - b0))) for x, b0 in zip(params, before))
    finite = all(np.all(np.isfinite(x.numpy())) for x in params)
    print(f"loss {losses[0]:.4f} -> {losses[-1]:.4f} over {steps} jitted Adam steps; first step "
          f"max |param delta| {first:.3e} (lr 3e-3); params finite {finite}")
    ok &= losses[-1] < losses[0] and 0 < first <= 3.01e-3 and finite
    print("SANITY " + ("OK" if ok else "FAIL"))


def main():
    cmd = sys.argv[1]
    if cmd == "prep":
        cmd_prep(*sys.argv[2:7])
    elif cmd == "sanity":
        cmd_sanity(sys.argv[2], sys.argv[3])
    elif cmd == "train":
        cmd_train(*sys.argv[2:6])
    elif cmd == "gate":
        cmd_gate(*sys.argv[2:6])
    elif cmd == "valgate":
        cmd_gate(*sys.argv[2:6], which="val")
    elif cmd == "select":
        cmd_select(sys.argv[2], *sys.argv[3:])


if __name__ == "__main__":
    main()
