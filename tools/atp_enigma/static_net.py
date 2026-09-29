#!/usr/bin/env python3
# Neural clause selection, phase 0c: a STATIC listwise head, scored once
# per critical pair (CP) at push time from what the engine has then,
# plus the pre-registered phase-0c gate
# (~/.cache/thvm/casc/J13/neural_p1/gate_spec_0c.txt).
#
# Score of CP c (fixed at push):
#   s(c) = -kappa log1p(pri(c)) + W2 relu(W1 z(c) + b1) + b2
# pri(c) = cp_pri at push; x(c) = [the engine's 14 CP features at push
# (thvm_atp_cp_features; the history export's 'F' records, armed by
# THVM_ATP_HISTORY_CPFEAT=1) ; log1p(pri(c))]; z(c) = (x(c) - mu) / sd
# with train statistics.  W2 and b2 start at zero and kappa at 1, so the
# untrained head ranks by the push priority, i.e. as the engine does
# wherever the lane never re-scores a queued CP.
#
# Objective, sampling, batching and the training loop are phase 0b's
# (listwise_net.py): at a sampled selection step t, -log of the softmax
# mass of the pending proof pairs within the reconstructed queue.
#
# Usage:
#   static_net.py train     <npz_dir> <pool.tsv> <out_prefix> <S16|S32>
#   static_net.py valgate   <model_prefix> <npz_dir> <pool.tsv> <out_prefix>
#   static_net.py gate      <model_prefix> <npz_dir> <pool.tsv> <out_prefix>
#   static_net.py select    <out.tsv> <ratios.tsv>...
#   static_net.py sanity    <npz_dir> <pool.tsv>
#   static_net.py featcheck <npz_dir> <pool.tsv> <out.txt>
import os
import sys
import time

import numpy as np
from tinygrad import Tensor, TinyJit, nn
from tinygrad.nn.state import get_parameters, get_state_dict, load_state_dict, safe_load, safe_save

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from history_net import (Problem, bootstrap, candidate_steps, fix_levels, gate_problem, is_val,
                         load_pool, load_problems, make_batches, pow2, train_mode, validate_levels)
from listwise_net import BIG, H_TOP, K_U, S_STEPS, list_loss, list_loss_ref, sample_problem

N_CPF = 14            # thvm_atp_cp_features (ATP_CP_FEATURE_DIM)
N_X = N_CPF + 1       # + log1p(push priority)
VARIANTS = {"S16": 16, "S32": 32}


# --------------------------------------------------------------------------
# inputs

def static_x(p):
    """Per-CP raw inputs: the 14 push features, then log1p(push priority)."""
    return np.concatenate([p.cpfeat, np.log1p(p.pri.astype(np.float64))[:, None]], 1)


def eligible(p):
    """Every kept CP carries its push features."""
    return p.cpfeat is not None and bool(np.all(p.has_cpfeat))


class StaticStd:
    """Input standardizer fitted on the train problems' CP rows."""

    def __init__(self, probs=None, arrays=None):
        if arrays is not None:
            self.mu, self.sd = arrays
            return
        x = np.concatenate([static_x(p) for p in probs])
        self.mu = x.mean(0)
        self.sd = x.std(0) + 1e-6


def member_arrays(p, views, rows, lw, S=S_STEPS):
    """Per (step slot, row): log-weight (-BIG off the queue) and positive
    log-mask.  Slots past the drawn steps repeat them (loss weight 0)."""
    n = len(rows)
    LW = np.full((S, n), -BIG)
    PM = np.full((S, n), -BIG)
    lab = p.label[rows]
    for k in range(S):
        a = views[k % len(views)][0][rows]
        LW[k] = np.where(a, lw, -BIG)
        PM[k] = np.where(a & lab, 0.0, -BIG)
    return LW, PM


# --------------------------------------------------------------------------
# batches and model

class StaticBatch:
    """A training batch with FIXED shapes across draws (one TinyJit
    capture): problem b owns rows [off_b, off_b + n_pos_b + S H_TOP + K_U),
    the rest padding; step slot k is every problem's k-th drawn step."""

    def __init__(self, probs, std):
        self.probs, self.std = probs, std
        self.cand = [candidate_steps(p) for p in probs]
        self.Z = [((static_x(p) - std.mu) / std.sd).astype(np.float32) for p in probs]
        self.base = [np.log1p(p.pri.astype(np.float64)).astype(np.float32) for p in probs]
        self.seg = np.array([int(p.label.sum()) + S_STEPS * H_TOP + K_U for p in probs])
        self.off = np.concatenate([[0], np.cumsum(self.seg)])
        self.R = pow2(int(self.seg.sum()))
        self.jit = None

    def draw(self, rng):
        """Numpy inputs of one step: Z, BASE, LW, PM, SEG, SW."""
        B, R, S = len(self.probs), self.R, S_STEPS
        Z = np.zeros((R, N_X), np.float32)
        BASE = np.zeros(R, np.float32)
        LW = np.full((S, R), -BIG, np.float32)
        PM = np.full((S, R), -BIG, np.float32)
        SEG = np.zeros((S, R), np.int32)
        SW = np.zeros((S, B), np.float32)
        self.last = []
        for b, p in enumerate(self.probs):
            steps, views, rows, lw = sample_problem(p, self.cand[b], rng)
            lwa, pma = member_arrays(p, views, rows, lw)
            c0, n = int(self.off[b]), len(rows)
            sl = slice(c0, c0 + n)
            Z[sl] = self.Z[b][rows]
            BASE[sl] = self.base[b][rows]
            LW[:, sl] = lwa
            PM[:, sl] = pma
            SEG[:, int(self.off[b]):int(self.off[b + 1])] = b
            SW[:len(steps), b] = 1.0 / (B * len(steps))
            self.last.append((steps, rows, lwa, pma))
        return Z, BASE, LW, PM, SEG, SW

    def sample(self, rng):
        return [Tensor(x).realize() for x in self.draw(rng)]


class StaticNet:
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

    def scores(self, z, base):
        return self.out(self.h1(z).relu()).reshape(-1) - self.kappa * base

    def loss(self, z, base, lw, pm, seg, sw):
        s = self.scores(z, base)
        S, R = lw.shape
        return list_loss(s.reshape(1, R).expand(S, R), lw, pm, seg, sw)


def save_model(prefix, model, std, meta):
    safe_save(get_state_dict(model), prefix + ".safetensors")
    np.savez(prefix + "_std.npz", mu=std.mu, sd=std.sd, **meta)


def load_model(prefix):
    m = dict(np.load(prefix + "_std.npz"))
    model = StaticNet(int(m["hid"]))
    load_state_dict(model, safe_load(prefix + ".safetensors"))
    return model, StaticStd(arrays=(m["mu"], m["sd"])), m


def numpy_scores(model, std, p):
    """s(c) for every kept CP of p, in float64 from the trained weights."""
    z = (static_x(p) - std.mu) / std.sd
    W1 = model.h1.weight.numpy().astype(np.float64)
    b1 = model.h1.bias.numpy().astype(np.float64)
    W2 = model.out.weight.numpy().astype(np.float64)[0]
    b2 = float(model.out.bias.numpy()[0])
    kappa = float(model.kappa.numpy()[0])
    return np.maximum(z @ W1.T + b1, 0.0) @ W2 + b2 - kappa * np.log1p(p.pri.astype(np.float64))


# --------------------------------------------------------------------------
# training

def split_problems(npz_dir, pool_path, max_levels):
    split = load_pool(pool_path)
    names = sorted(n for n, s in split.items() if s == "train")
    out, dropped = [], 0
    for nm in ([n for n in names if not is_val(n)], [n for n in names if is_val(n)]):
        ps = load_problems(npz_dir, nm)
        keep = [p for p in ps if eligible(p)]
        dropped += len(ps) - len(keep)
        out.append([p for p in keep if p.n_levels <= max_levels and len(candidate_steps(p))])
    return out[0], out[1], dropped


def cmd_train(npz_dir, pool_path, out_prefix, variant, epochs=30, patience=5, lr=1e-3,
              seed=0, max_levels=100):
    hid = VARIANTS[variant]
    t0 = time.time()
    tr, va, dropped = split_problems(npz_dir, pool_path, max_levels)
    Tensor.manual_seed(seed)
    rng = np.random.default_rng(seed)
    std = StaticStd(tr)
    model = StaticNet(hid)
    opt = nn.optim.Adam(get_parameters(model), lr=lr)
    tb = [StaticBatch([tr[i] for i in b], std) for b in make_batches(tr)]
    vb = [StaticBatch([va[i] for i in b], std) for b in make_batches(va)]
    print(f"{variant}: train {len(tr)} problems ({len(tb)} batches) val {len(va)} "
          f"({len(vb)} batches), {dropped} without push features, loaded in "
          f"{time.time()-t0:.0f}s; input mu {np.round(std.mu, 3).tolist()} sd "
          f"{np.round(std.sd, 3).tolist()}", flush=True)
    for fb in tb:
        def step(*ins, fb=fb):
            loss = model.loss(*ins)
            opt.zero_grad()
            loss.backward()
            opt.step()
            return loss.realize()
        fb.jit = TinyJit(step)
    vrng = np.random.default_rng(1234)
    for fb in vb:
        def fwd(*ins, fb=fb):
            return model.loss(*ins).realize()
        fb.jit = TinyJit(fwd)
        fb.fixed = fb.sample(vrng)
    ntr = sum(len(fb.probs) for fb in tb)
    nva = sum(len(fb.probs) for fb in vb)
    meta = dict(hid=hid, variant=variant, seed=seed, lr=lr, S=S_STEPS, h=H_TOP, k_u=K_U,
                max_levels=max_levels)
    vl0 = sum(fb.jit(*fb.fixed).item() * len(fb.probs) for fb in vb) / nva
    print(f"epoch -1 (the push priority alone, kappa 1) val {vl0:.4f}", flush=True)
    best = (np.inf, -1)
    for ep in range(epochs):
        te = time.time()
        tl = 0.0
        with train_mode():
            for bi in rng.permutation(len(tb)):
                fb = tb[bi]
                tl += fb.jit(*fb.sample(rng)).item() * len(fb.probs)
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
# gate (gate_spec_0c.txt)

def gate_rows(model, std, npz_dir, names):
    per, rows, skipped = [], [], 0
    for nm in names:
        path = os.path.join(npz_dir, nm + ".npz")
        if not os.path.exists(path):
            continue
        p = Problem(path)
        if not validate_levels(p):
            fix_levels(p)
        if p.label.sum() == 0:
            continue
        if not eligible(p):
            skipped += 1
            continue
        r = gate_problem(p, numpy_scores(model, std, p))
        if r is None:
            continue
        med = {k: float(np.median(r["pct"][k])) for k in r["pct"]}
        hit = {k: float(np.mean(r["hit"][k])) for k in r["hit"]}
        per.append((nm, med, hit, len(r["hit"]["model"]), len(r["pct"]["model"]), p.rate,
                    int(p.label.sum()), p.n_sel))
        rows.append(r)
    return per, rows, skipped


def cmd_gate(model_prefix, npz_dir, pool_path, out_prefix, which="heldout"):
    model, std, meta = load_model(model_prefix)
    split = load_pool(pool_path)
    if which == "heldout":
        names = sorted(n for n, s in split.items() if s == "heldout")
    else:   # the validation slice carved from train (model selection only)
        names = sorted(n for n, s in split.items() if s == "train" and is_val(n))
    per, rows, skipped = gate_rows(model, std, npz_dir, names)
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
             f"{int(meta['best_epoch'])}); eligible problems: {len(per)} (skipped without "
             f"push features: {skipped})"]
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
    pass_m, pass_h = ci_rm[1] < 1.0, ci_rh[1] < 1.0
    lines.append(f"ratio median model/engine {r_med:.4f} CI [{ci_rm[0]:.4f}, {ci_rm[1]:.4f}] "
                 f"(CI upper end < 1: {'yes' if pass_m else 'no'})")
    lines.append(f"ratio miss model/engine {r_miss:.4f} CI [{ci_rh[0]:.4f}, {ci_rh[1]:.4f}] "
                 f"(CI upper end < 1: {'yes' if pass_h else 'no'})")
    lines.append(f"selection score max(r_med, r_miss) {max(r_med, r_miss):.4f}")
    lines.append("GATE 0c: " + ("PASS" if (pass_m and pass_h) else "FAIL"))
    bm = np.sum(meds["model"] < meds["engine"]); wm = np.sum(meds["model"] > meds["engine"])
    bh = np.sum(miss["model"] < miss["engine"]); wh = np.sum(miss["model"] > miss["engine"])
    lines.append(f"per problem: median better {bm} / worse {wm} / tie {len(per) - bm - wm}; "
                 f"miss better {bh} / worse {wh} / tie {len(per) - bh - wh}")
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
    with open(out_prefix + "_ratios.tsv", "w") as f:
        f.write("variant\tprefix\tbest_epoch\tbest_val\tr_med\tr_miss\tmax\n")
        f.write(f"{str(meta['variant'])}\t{model_prefix}\t{int(meta['best_epoch'])}\t"
                f"{float(meta['best_val']):.4f}\t{r_med:.4f}\t{r_miss:.4f}\t"
                f"{max(r_med, r_miss):.4f}\n")


def cmd_select(out_path, *ratio_paths):
    """The fixed rule: lowest max(r_med, r_miss) on validation, ties to
    the lower r_med."""
    rows = []
    for rp in ratio_paths:
        with open(rp) as f:
            next(f)
            v, pre, ep, bv, rm, rh, mx = next(f).rstrip("\n").split("\t")
            rows.append((float(mx), float(rm), v, pre, ep, bv, rh))
    rows.sort()
    with open(out_path, "w") as f:
        f.write("variant\tprefix\tbest_epoch\tbest_val\tr_med\tr_miss\tmax\tstatus\n")
        for i, (mx, rm, v, pre, ep, bv, rh) in enumerate(rows):
            f.write(f"{v}\t{pre}\t{ep}\t{bv}\t{rm:.4f}\t{rh}\t{mx:.4f}\t"
                    f"{'CHOSEN' if i == 0 else ''}\n")
    print(open(out_path).read().rstrip())


# --------------------------------------------------------------------------
# featcheck: coverage of the push features and their agreement with the
# 'P' record's structural columns where both define the same quantity

def cmd_featcheck(npz_dir, pool_path, out_path):
    split = load_pool(pool_path)
    tot = {"problems": 0, "rows": 0, "with_F": 0}
    bad = {"size": 0, "depth": 0, "vars": 0, "occ": 0, "add": 0, "age": 0}
    cmp_rows = 0
    lines = []
    for nm in sorted(split):
        path = os.path.join(npz_dir, nm + ".npz")
        if not os.path.exists(path):
            continue
        d = np.load(path)
        if "cpfeat" not in d:
            lines.append(f"{nm}: no cpfeat array")
            continue
        tot["problems"] += 1
        n = len(d["seq"])
        tot["rows"] += n
        hf = d["has_cpfeat"].astype(bool)
        tot["with_F"] += int(hf.sum())
        # rows whose 'P' content was never re-packed (no reweight) and
        # whose structural features are valid
        feat = d["feat"].astype(np.int64)
        ok = hf & ((d["flags"] & 0x08) != 0) & ~np.isin(d["seq"], d["w_seq"])
        f = d["cpfeat"].astype(np.float64)
        sat = (feat[:, :9] >= 0xffff).any(1)
        ok &= ~sat
        cmp_rows += int(ok.sum())
        bad["size"] += int(np.sum(ok & (f[:, 0] != feat[:, 0] + feat[:, 1])))
        bad["depth"] += int(np.sum(ok & (f[:, 1] != np.maximum(feat[:, 2], feat[:, 3]) - 1)))
        vok = ok & ((feat[:, 9] & 128) == 0) & (feat[:, 4] <= 64)
        bad["vars"] += int(np.sum(vok & (f[:, 2] != feat[:, 4])))
        bad["occ"] += int(np.sum(ok & (f[:, 3] != feat[:, 5])))
        bad["add"] += int(np.sum(ok & (f[:, 4] != f[:, 0])))
        bad["age"] += int(np.sum(ok & (f[:, 8] != d["seq"].astype(np.float64))))
    lines.append(f"problems {tot['problems']} rows {tot['rows']} with push features "
                 f"{tot['with_F']} ({tot['with_F'] / max(1, tot['rows']):.6f})")
    lines.append(f"compared rows (P features valid, never reweighted): {cmp_rows}")
    lines.append("mismatches: " + " ".join(f"{k}={v}" for k, v in bad.items()))
    lines.append("FEATCHECK " + ("OK" if all(v == 0 for v in bad.values())
                                 and tot["with_F"] == tot["rows"] else "MISMATCH"))
    txt = "\n".join(lines)
    print(txt)
    with open(out_path, "w") as f:
        f.write(txt + "\n")


# --------------------------------------------------------------------------
# sanity: the loss matches a numpy reference, the untrained head ranks
# like the engine wherever the lane never re-scores, and jitted Adam
# lowers the loss

def cmd_sanity(npz_dir, pool_path, n_probs=6, steps=30):
    split = load_pool(pool_path)
    names = sorted(n for n, s in split.items() if s == "train" and not is_val(n))
    probs = [p for p in load_problems(npz_dir, names[:60]) if eligible(p)
             and len(candidate_steps(p))]
    probs = sorted(probs, key=lambda p: -len(p.seq))[:n_probs]
    Tensor.manual_seed(0)
    rng = np.random.default_rng(0)
    std = StaticStd(probs)
    ok = True
    model = StaticNet(16)
    model.out.weight.assign(Tensor.uniform(1, 16, low=-0.5, high=0.5)).realize()
    fb = StaticBatch(probs, std)
    np_ins = fb.draw(rng)
    ins = [Tensor(x).realize() for x in np_ins]
    s = model.scores(ins[0], ins[1]).numpy()
    got = model.loss(*ins).item()
    S = np_ins[2].shape[0]
    ref = list_loss_ref(np.repeat(s[None, :], S, 0), np_ins[2], np_ins[3], np_ins[5], fb.off)
    rel = abs(got - ref) / max(1e-9, abs(ref))
    # batch path against the gate's numpy path
    ds = 0.0
    for b, p in enumerate(probs):
        _, rows, _, _ = fb.last[b]
        c0 = int(fb.off[b])
        ds = max(ds, float(np.max(np.abs(s[c0:c0 + len(rows)]
                                         - numpy_scores(model, std, p)[rows]))))
    print(f"loss tinygrad {got:.6f} numpy {ref:.6f} (rel {rel:.1e}); batch vs gate score "
          f"max |diff| {ds:.1e}")
    ok &= rel < 1e-4 and ds < 1e-3
    model = StaticNet(16)
    for p in probs:
        r = gate_problem(p, numpy_scores(model, std, p))
        eq = (np.allclose(r["pct"]["model"], r["pct"]["engine"])
              and r["hit"]["model"] == r["hit"]["engine"])
        exact = p.rate == 1.0 and len(p.w_seq) == 0
        print(f"  {p.name}: rate {p.rate:.3g} reweights {len(p.w_seq)} untrained median "
              f"{np.median(r['pct']['model']):.4f} engine {np.median(r['pct']['engine']):.4f} "
              f"{'identical' if eq else 'DIFFERENT'}")
        ok &= eq or not exact
    params = get_parameters(model)
    opt = nn.optim.Adam(params, lr=3e-3)
    fixed = fb.sample(rng)
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
    print(f"loss {losses[0]:.4f} -> {losses[-1]:.4f} over {steps} jitted Adam steps; first "
          f"step max |param delta| {first:.3e} (lr 3e-3); params finite {finite}")
    ok &= losses[-1] < losses[0] and 0 < first <= 3.01e-3 and finite
    print("SANITY " + ("OK" if ok else "FAIL"))


def main():
    cmd = sys.argv[1]
    if cmd == "train":
        cmd_train(*sys.argv[2:6])
    elif cmd == "gate":
        cmd_gate(*sys.argv[2:6])
    elif cmd == "valgate":
        cmd_gate(*sys.argv[2:6], which="val")
    elif cmd == "select":
        cmd_select(sys.argv[2], *sys.argv[3:])
    elif cmd == "sanity":
        cmd_sanity(sys.argv[2], sys.argv[3])
    elif cmd == "featcheck":
        cmd_featcheck(sys.argv[2], sys.argv[3], sys.argv[4])


if __name__ == "__main__":
    main()
