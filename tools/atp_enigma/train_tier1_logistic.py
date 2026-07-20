#!/usr/bin/env python3
# Train the ENIGMA Tier-1 logistic critical-pair scorer on the labelled
# feature TSVs exported by THVM_ATP_ENIGMA_EXPORT (see README.md).  Plain
# numpy -- no sklearn/scipy dependency.  Emits a whitespace-separated f64
# weights blob in thvm_atp_set_learned_scorer's LINEAR layout, loadable at
# run time via THVM_ATP_ENIGMA_WEIGHTS / --enigma-weights.
#
# Blob layout (45 values): kind(=1 LINEAR) hidden(=0) mean[14] inv_std[14]
# w_lin[14] b.  The C forward standardizes z=(feat-mean)*inv_std then
# scores b + w.z, so training standardizes freely and the engine stays a
# plain dot product.
#
# Usage:
#   train_tier1_logistic.py <train.tsv[,train2.tsv]> <heldout.tsv> \
#                           <out.weights> [cw_mult] [l2] [drop_idx,...]
#   cw_mult  extra multiplier on the (already class-balanced) positive
#            weight; 1.0 = pure balanced (best in practice; >1 hurts P@k).
#   l2       L2 penalty (1e-3 default-good).
#   drop_idx comma list of feature indices to neutralize (inv_std=0).
#
# EMPIRICAL VERDICT (2026-07, casc29+casc30 train / cascJ12 held out):
#   The linear model RANKS proof-relevant CPs well offline (held-out AUC
#   ~0.92) but does NOT transfer to DRIVING selection: deployed via
#   --enigma it regresses easy provers (the `age` recency feature drives a
#   depth-first, heap-exploding order) and solves none of the hard CASC
#   UEQ targets (GRP690-1 et al.).  This is the ENIGMA train/deploy
#   distribution-shift gap -- offline ranking on baseline-recorded CPs is
#   not online selection quality.  The baked-in ATP_LEARNED_W scorer
#   (goal-connection + orientability dominant, tiny age weight) does not
#   explode but also cracks no target.  A viable learned lane needs
#   DAgger-style retraining on self-generated CPs and/or richer
#   (symbol-pattern) features than the 14 hand columns.
import sys, numpy as np

DIM = 14
FEATS = ["size_sum", "max_depth", "n_distinct_vars", "n_var_occ",
         "weight_add", "weight_gt", "weight_mix2", "goal_weight", "age",
         "top_symbol_l", "top_symbol_r", "shares_goal_sub", "orientable",
         "unif_measure"]


def load(paths):
    X, y, bad = [], [], 0
    for p in paths:
        with open(p) as f:
            for line in f:
                line = line.rstrip("\n")
                if not line or line.startswith("label"):
                    continue                      # header line per problem
                parts = line.split("\t")
                if len(parts) != DIM + 1:
                    bad += 1
                    continue
                try:
                    row = [float(x) for x in parts]
                except ValueError:
                    bad += 1
                    continue
                y.append(row[0])
                X.append(row[1:])
    return np.asarray(X, float), np.asarray(y, float), bad


def sigmoid(z):
    return np.where(z >= 0, 1.0 / (1.0 + np.exp(-z)),
                    np.exp(z) / (1.0 + np.exp(z)))


def auc(y, s):
    order = np.argsort(s, kind="mergesort")
    ranks = np.empty(len(s), float)
    ss = s[order]
    i, n = 0, len(s)
    while i < n:
        j = i
        while j < n and ss[j] == ss[i]:
            j += 1
        ranks[order[i:j]] = (i + j - 1) / 2.0 + 1.0
        i = j
    npos = y.sum()
    nneg = len(y) - npos
    if npos == 0 or nneg == 0:
        return float("nan")
    return (ranks[y == 1].sum() - npos * (npos + 1) / 2.0) / (npos * nneg)


def prec_at_k(y, s, k):
    return y[np.argsort(-s, kind="mergesort")[:k]].mean()


def train(Xtr, ytr, cw_mult=1.0, lam=1e-3, iters=4000, lr=0.3, drop=()):
    mean = Xtr.mean(0)
    std = Xtr.std(0)
    inv = np.where(std > 1e-9, 1.0 / std, 0.0)
    for j in drop:
        inv[j] = 0.0
    Z = (Xtr - mean) * inv
    n, d = Z.shape
    npos = ytr.sum()
    nneg = n - npos
    c = np.where(ytr == 1, cw_mult * n / (2.0 * npos), n / (2.0 * nneg))
    w = np.zeros(d)
    b = 0.0
    mw = np.zeros(d); vw = np.zeros(d); mb = 0.0; vb = 0.0
    b1, b2, eps = 0.9, 0.999, 1e-8
    for t in range(1, iters + 1):
        p = sigmoid(Z @ w + b)
        g = c * (p - ytr)
        gw = Z.T @ g / n + lam * w
        gb = g.sum() / n
        mw = b1 * mw + (1 - b1) * gw; vw = b2 * vw + (1 - b2) * gw * gw
        mb = b1 * mb + (1 - b1) * gb; vb = b2 * vb + (1 - b2) * gb * gb
        w -= lr * (mw / (1 - b1**t)) / (np.sqrt(vw / (1 - b2**t)) + eps)
        b -= lr * (mb / (1 - b1**t)) / (np.sqrt(vb / (1 - b2**t)) + eps)
    return mean, inv, w, b


def main():
    tr = sys.argv[1].split(",")
    te = sys.argv[2]
    out = sys.argv[3]
    cw = float(sys.argv[4]) if len(sys.argv) > 4 else 1.0
    lam = float(sys.argv[5]) if len(sys.argv) > 5 else 1e-3
    drop = tuple(int(x) for x in sys.argv[6].split(",")) \
        if len(sys.argv) > 6 and sys.argv[6] else ()
    Xtr, ytr, badtr = load(tr)
    Xte, yte, badte = load([te])
    print(f"train rows={len(ytr)} pos={int(ytr.sum())} "
          f"({100*ytr.mean():.3f}%) bad={badtr}")
    print(f"heldout rows={len(yte)} pos={int(yte.sum())} "
          f"({100*yte.mean():.3f}%) bad={badte}")
    mean, inv, w, b = train(Xtr, ytr, cw_mult=cw, lam=lam, drop=drop)
    if drop:
        print("dropped feats:", [FEATS[j] for j in drop])
    str_ = (Xtr - mean) * inv @ w + b
    ste = (Xte - mean) * inv @ w + b
    print(f"cw_mult={cw} l2={lam}")
    print(f"  train AUC={auc(ytr, str_):.4f}  heldout AUC={auc(yte, ste):.4f}")
    for k in (100, 500, 1000):
        print(f"  heldout P@{k}={prec_at_k(yte, ste, k):.3f} "
              f"(base {yte.mean():.4f})")
    print("  top |w|:", [FEATS[i] for i in np.argsort(-np.abs(w))[:5]])
    blob = [1.0, 0.0] + list(mean) + list(inv) + list(w) + [float(b)]
    assert len(blob) == 45, len(blob)
    with open(out, "w") as f:
        f.write(" ".join(f"{v:.9g}" for v in blob) + "\n")
    print(f"  wrote {len(blob)} params -> {out}")


if __name__ == "__main__":
    main()
