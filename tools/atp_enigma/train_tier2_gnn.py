#!/usr/bin/env python3
# Train the ENIGMA Tier-2 graph convolutional network (GCN) critical-pair
# scorer on the JSONL graph datasets exported by
# THVM_ATP_ENIGMA_GRAPH_EXPORT (see README.md).  Pure numpy -- CPU only, no
# torch/sklearn.  The forward pass matches the in-engine C scorer
# (atp_gnn_forward in src/atp/_.c) and the WL reference (atpGnnForwardLogits
# in wl/THVMLink/Kernel/ATP/ATP.wl) OP-FOR-OP, so a model trained here and
# saved as .safetensors drops straight into the engine via
# THVM_ATP_GNN_ASSET / TAtpSetGnnScorer and scores identically online.  The
# `parity` subcommand asserts this against bin/gnn_score_dump.
#
# Architecture (R rounds, hidden H, node-feature dim F = ATP_CPG_FEAT_DIM,
# 12 in the phase-2 schema; the forward is feature-dim agnostic):
#   h = X
#   for r in 0..R:  h = relu(A.h.W1[r] + h.Ws[r] + Bh[r])
#   pooled = masked_mean(h)                         # over real nodes
#   logits = pooled.Wout + Bout                     # (B, 2)
#   score  = logits[:,1] - logits[:,0]              # proof-relevance
# A is the row-normalised adjacency with self-loops (edges symmetrised),
# exactly build_host / atpGnnTensors.  Two-class softmax cross-entropy,
# class-balanced minibatches (positives are ~1-2% of processed CPs).
#
# Usage:
#   train_tier2_gnn.py train  <train.jsonl[,train2,...]> <heldout.jsonl> \
#                             <out.safetensors> [H R steps lr neg_cap seed]
#   train_tier2_gnn.py parity <graphs.jsonl> <gnn_score_dump_bin> [n H R seed]
#   train_tier2_gnn.py score  <model.safetensors> <graphs.jsonl>   # -> AUC
#
# Blob / safetensors layout consumed by thvm_atp_load_gnn_safetensors:
# named F32 tensors W1_<r>/Ws_<r>/Bh_<r> (r 0-based) then Wout/Bout, with
# Rounds/Hidden in the file's __metadata__.
import sys, os, json, struct, subprocess, tempfile
import numpy as np

F = 12                     # ATP_CPG_FEAT_DIM (phase-2 schema; was 6)
NODE_CAP = 64              # ATP_GNN_N_CAP / WL NodeCap

# ---------------------------------------------------------------- dataset

def load_graphs(paths, neg_cap=200000, seed=0):
    """Stream JSONL graphs; keep every positive, reservoir-sample negatives
    to at most neg_cap (bounds memory + training time under the ~1.5%
    positive rate).  Each graph -> (x: (n,6) f32, edges: (m,2) i32, y)."""
    rng = np.random.default_rng(seed)
    pos, neg = [], []
    neg_seen = 0
    for path in paths:
        with open(path) as fh:
            for line in fh:
                line = line.strip()
                if not line:
                    continue
                g = json.loads(line)
                x = np.asarray(g["x"], dtype=np.float32)
                e = (np.asarray(g["e"], dtype=np.int32)
                     if g["e"] else np.zeros((0, 2), np.int32))
                rec = (x, e, int(g["y"]))
                if rec[2] == 1:
                    pos.append(rec)
                else:
                    neg_seen += 1
                    if len(neg) < neg_cap:
                        neg.append(rec)
                    else:                              # reservoir replacement
                        j = rng.integers(0, neg_seen)
                        if j < neg_cap:
                            neg[j] = rec
    return pos, neg, neg_seen


def densify(batch, N=NODE_CAP):
    """Densify a list of (x, edges, y) into padded (X, A, mask, nn) exactly
    as the C build_host / WL atpGnnTensors: truncate to N nodes, symmetrise
    edges, add self-loops, row-normalise; mask + node count for the pool.
    The feature width is read from the data (not hard-coded), so a dataset
    exported at any ATP_CPG_FEAT_DIM densifies correctly."""
    B = len(batch)
    f = next((b[0].shape[1] for b in batch if b[0].shape[0] > 0), F)
    X = np.zeros((B, N, f), np.float32)
    A = np.zeros((B, N, N), np.float32)
    mask = np.zeros((B, 1, N), np.float32)
    nn = np.ones((B, 1, 1), np.float32)
    for bi, (x, e, _y) in enumerate(batch):
        n = min(x.shape[0], N)
        if n == 0:
            continue
        X[bi, :n] = x[:n]
        mask[bi, 0, :n] = 1.0
        nn[bi, 0, 0] = float(n)
        adj = A[bi]
        for s, d in e:
            if s < n and d < n:
                adj[s, d] = 1.0
                adj[d, s] = 1.0
        for i in range(n):
            adj[i, i] = 1.0
        rs = adj.sum(1, keepdims=True)
        np.divide(adj, rs, out=adj, where=rs > 0.0)
    return X, A, mask, nn

# ---------------------------------------------------------------- model

def glorot(shape, rng):
    fan_in, fan_out = shape[0], shape[-1]
    lim = np.sqrt(6.0 / (fan_in + fan_out))
    return rng.uniform(-lim, lim, size=shape).astype(np.float32)


def init_model(H, R, seed=0, feat=F):
    rng = np.random.default_rng(seed)
    W1 = [glorot((feat if r == 0 else H, H), rng) for r in range(R)]
    Ws = [glorot((feat if r == 0 else H, H), rng) for r in range(R)]
    Bh = [np.zeros(H, np.float32) for _ in range(R)]
    Wout = glorot((H, 2), rng)
    Bout = np.zeros(2, np.float32)
    return dict(W1=W1, Ws=Ws, Bh=Bh, Wout=Wout, Bout=Bout, H=H, R=R, F=feat)


def forward(m, X, A, mask, nn):
    """(B,2) logits + a cache for backward.  float32 throughout to track
    the engine's f32 UOP runtime."""
    R = m["R"]
    caches = []
    h = X
    for r in range(R):
        ah = A @ h                                  # (B,N,d)
        pre = (ah @ m["W1"][r] + h @ m["Ws"][r] + m["Bh"][r]).astype(np.float32)
        caches.append((h, ah, pre))
        h = np.maximum(pre, 0.0).astype(np.float32)
    pooled_num = (mask @ h)[:, 0, :]                # (B,H)
    inv = (1.0 / nn[:, 0, 0]).astype(np.float32)    # (B,)
    pooled = (pooled_num * inv[:, None]).astype(np.float32)
    logits = (pooled @ m["Wout"] + m["Bout"]).astype(np.float32)
    cache = dict(caches=caches, A=A, mask=mask, inv=inv, pooled=pooled)
    return logits, cache


def backward(m, cache, dlogits):
    R = m["R"]
    g = dict(W1=[None] * R, Ws=[None] * R, Bh=[None] * R)
    pooled, inv, mask, A = cache["pooled"], cache["inv"], cache["mask"], cache["A"]
    g["Wout"] = pooled.T @ dlogits                  # (H,2)
    g["Bout"] = dlogits.sum(0)                       # (2,)
    dpooled = dlogits @ m["Wout"].T                  # (B,H)
    dpooled_num = dpooled * inv[:, None]             # (B,H)
    dh = mask[:, 0, :, None] * dpooled_num[:, None, :]   # (B,N,H)
    for r in reversed(range(R)):
        h, ah, pre = cache["caches"][r]
        dpre = dh * (pre > 0.0)                       # relu'
        g["Bh"][r] = dpre.sum(axis=(0, 1))
        g["W1"][r] = np.einsum("bni,bnh->ih", ah, dpre)
        g["Ws"][r] = np.einsum("bni,bnh->ih", h, dpre)
        dah = dpre @ m["W1"][r].T                     # (B,N,d)
        dh_self = dpre @ m["Ws"][r].T                 # (B,N,d)
        dh_neigh = np.einsum("bnm,bnh->bmh", A, dah)  # A^T . dah (A row-norm'd)
        dh = dh_self + dh_neigh
    return g


def softmax_ce(logits, y, cw):
    mx = logits.max(1, keepdims=True)
    e = np.exp(logits - mx)
    sm = e / e.sum(1, keepdims=True)
    idx = np.arange(len(y))
    tgt = np.zeros_like(sm)
    tgt[idx, y] = 1.0
    loss = float((cw * -np.log(sm[idx, y] + 1e-12)).sum() / len(y))
    dlogits = ((cw[:, None] * (sm - tgt)) / len(y)).astype(np.float32)
    return loss, dlogits


def score_all(m, graphs, chunk=256):
    """logit_pos - logit_neg for every graph, in bounded chunks."""
    out = np.empty(len(graphs), np.float32)
    for i in range(0, len(graphs), chunk):
        sub = graphs[i:i + chunk]
        X, A, mask, nn = densify(sub)
        logits, _ = forward(m, X, A, mask, nn)
        out[i:i + len(sub)] = logits[:, 1] - logits[:, 0]
    return out

# ---------------------------------------------------------------- metrics

def auc(y, s):
    y = np.asarray(y)
    order = np.argsort(s, kind="mergesort")
    ss = s[order]
    ranks = np.empty(len(s), float)
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
    y = np.asarray(y)
    k = min(k, len(y))
    return float(y[np.argsort(-s, kind="mergesort")[:k]].mean())

# ---------------------------------------------------------------- safetensors

def save_safetensors(path, m):
    tensors = {}
    for r in range(m["R"]):
        tensors[f"W1_{r}"] = m["W1"][r]
        tensors[f"Ws_{r}"] = m["Ws"][r]
        tensors[f"Bh_{r}"] = m["Bh"][r]
    tensors["Wout"] = m["Wout"]
    tensors["Bout"] = m["Bout"]
    header, blob, off = {}, bytearray(), 0
    for name, arr in tensors.items():
        b = np.ascontiguousarray(arr, dtype="<f4").tobytes()
        header[name] = {"dtype": "F32", "shape": list(arr.shape),
                        "data_offsets": [off, off + len(b)]}
        blob += b
        off += len(b)
    feat = m["W1"][0].shape[0]
    header["__metadata__"] = {"Rounds": str(m["R"]), "Hidden": str(m["H"]),
                              "NMax": str(NODE_CAP), "Feat": str(feat)}
    hj = json.dumps(header, separators=(",", ":")).encode("utf-8")
    hj += b" " * ((8 - len(hj) % 8) % 8)             # tinygrad pads with spaces
    with open(path, "wb") as f:
        f.write(struct.pack("<Q", len(hj)))
        f.write(hj)
        f.write(bytes(blob))
    return path


def load_safetensors(path):
    with open(path, "rb") as f:
        hlen = struct.unpack("<Q", f.read(8))[0]
        header = json.loads(f.read(hlen))
        data = f.read()
    meta = header["__metadata__"]
    R, H = int(meta["Rounds"]), int(meta["Hidden"])
    # Feature dim is versioned in the header (Feat) and is also the first
    # axis of W1_0; read it there so a model saved at any schema loads
    # (old F=6 blobs included) without a code change.
    feat = int(meta.get("Feat", header["W1_0"]["shape"][0]))

    def get(name, shape):
        b, e = header[name]["data_offsets"]
        return np.frombuffer(data[b:e], dtype="<f4").reshape(shape).astype(np.float32)

    W1 = [get(f"W1_{r}", (feat if r == 0 else H, H)) for r in range(R)]
    Ws = [get(f"Ws_{r}", (feat if r == 0 else H, H)) for r in range(R)]
    Bh = [get(f"Bh_{r}", (H,)) for r in range(R)]
    return dict(W1=W1, Ws=Ws, Bh=Bh, Wout=get("Wout", (H, 2)),
                Bout=get("Bout", (2,)), H=H, R=R, F=feat)

# ---------------------------------------------------------------- train

def train(m, pos, neg, steps=800, bs=128, lr=0.01, seed=0):
    rng = np.random.default_rng(seed)
    # Adam state per parameter tensor (params alias the model arrays, so
    # the in-place updates below leave `m` trained).
    params = {}
    for r in range(m["R"]):
        params[f"W1_{r}"] = m["W1"][r]
        params[f"Ws_{r}"] = m["Ws"][r]
        params[f"Bh_{r}"] = m["Bh"][r]
    params["Wout"] = m["Wout"]
    params["Bout"] = m["Bout"]
    mom = {k: np.zeros_like(v) for k, v in params.items()}
    vel = {k: np.zeros_like(v) for k, v in params.items()}
    b1, b2, eps = 0.9, 0.999, 1e-8
    half = bs // 2
    loss0 = lossN = None
    for t in range(1, steps + 1):
        # class-balanced minibatch: half positives, half negatives.
        pi = rng.integers(0, len(pos), size=min(half, len(pos)))
        ni = rng.integers(0, len(neg), size=bs - len(pi))
        batch = [pos[i] for i in pi] + [neg[i] for i in ni]
        y = np.array([b[2] for b in batch])
        cw = np.ones(len(batch), np.float32)
        X, A, mask, nn = densify(batch)
        logits, cache = forward(m, X, A, mask, nn)
        loss, dlogits = softmax_ce(logits, y, cw)
        if t == 1:
            loss0 = loss
        lossN = loss
        grads = backward(m, cache, dlogits)
        gmap = {}
        for r in range(m["R"]):
            gmap[f"W1_{r}"] = grads["W1"][r]
            gmap[f"Ws_{r}"] = grads["Ws"][r]
            gmap[f"Bh_{r}"] = grads["Bh"][r]
        gmap["Wout"] = grads["Wout"]
        gmap["Bout"] = grads["Bout"]
        for k, p in params.items():
            gk = gmap[k].astype(np.float32)
            mom[k] = b1 * mom[k] + (1 - b1) * gk
            vel[k] = b2 * vel[k] + (1 - b2) * gk * gk
            mhat = mom[k] / (1 - b1 ** t)
            vhat = vel[k] / (1 - b2 ** t)
            p -= (lr * mhat / (np.sqrt(vhat) + eps)).astype(np.float32)
    # params alias the model arrays in place (Bh/Wout/Bout are the same
    # objects; W1/Ws lists hold the same arrays), so m is already updated.
    return loss0, lossN

# ---------------------------------------------------------------- subcommands

def cmd_train(argv):
    tr = argv[0].split(",")
    te = argv[1]
    out = argv[2]
    H = int(argv[3]) if len(argv) > 3 else 32
    R = int(argv[4]) if len(argv) > 4 else 3
    steps = int(argv[5]) if len(argv) > 5 else 800
    lr = float(argv[6]) if len(argv) > 6 else 0.01
    neg_cap = int(argv[7]) if len(argv) > 7 else 200000
    seed = int(argv[8]) if len(argv) > 8 else 0

    pos, neg, neg_seen = load_graphs(tr, neg_cap=neg_cap, seed=seed)
    print(f"train: pos={len(pos)} neg={len(neg)} (of {neg_seen} seen) "
          f"pos_rate={len(pos)/(len(pos)+neg_seen)*100:.3f}%")
    tep, ten, _ = load_graphs([te], neg_cap=10**9, seed=seed)
    test = tep + ten
    yte = [g[2] for g in test]
    print(f"heldout: rows={len(test)} pos={sum(yte)} "
          f"({100*np.mean(yte):.3f}%)")

    feat = next((g[0].shape[1] for g in pos + neg if g[0].shape[0] > 0), F)
    print(f"feature dim F={feat}")
    m = init_model(H, R, seed=seed, feat=feat)
    l0, lN = train(m, pos, neg, steps=steps, lr=lr, seed=seed)

    # train ranking AUC on a balanced sample the model actually saw.
    trs = pos + neg
    ytr = [g[2] for g in trs]
    str_ = score_all(m, trs)
    ste = score_all(m, test)
    print(f"H={H} R={R} steps={steps} lr={lr}  loss {l0:.4f} -> {lN:.4f}")
    print(f"  train AUC={auc(ytr, str_):.4f}  heldout AUC={auc(yte, ste):.4f}")
    base = float(np.mean(yte))
    for k in (100, 500, 1000):
        p = prec_at_k(yte, ste, k)
        print(f"  heldout P@{k}={p:.4f} (base {base:.4f}, lift {p/base:.1f}x)"
              if base > 0 else f"  heldout P@{k}={p:.4f}")
    save_safetensors(out, m)
    print(f"  wrote model (R={R} H={H}) -> {out}")


def cmd_parity(argv):
    graphs_path = argv[0]
    c_bin = argv[1]
    n = int(argv[2]) if len(argv) > 2 else 32
    H = int(argv[3]) if len(argv) > 3 else 8
    R = int(argv[4]) if len(argv) > 4 else 3
    seed = int(argv[5]) if len(argv) > 5 else 7

    # Read the first n graphs.
    graphs, lines = [], []
    with open(graphs_path) as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            g = json.loads(line)
            x = np.asarray(g["x"], dtype=np.float32)
            e = (np.asarray(g["e"], dtype=np.int32)
                 if g["e"] else np.zeros((0, 2), np.int32))
            graphs.append((x, e, int(g["y"])))
            lines.append(line)
            if len(graphs) >= n:
                break
    if not graphs:
        print("parity: no graphs"); sys.exit(1)

    feat = next((g[0].shape[1] for g in graphs if g[0].shape[0] > 0), F)
    m = init_model(H, R, seed=seed, feat=feat)
    with tempfile.TemporaryDirectory() as td:
        mp = os.path.join(td, "m.safetensors")
        gp = os.path.join(td, "g.jsonl")
        save_safetensors(mp, m)
        with open(gp, "w") as f:
            f.write("\n".join(lines) + "\n")
        res = subprocess.run([c_bin, gp, mp], capture_output=True, text=True)
        if res.returncode != 0:
            print("C tool failed:", res.stderr); sys.exit(1)
        c_scores = np.array([float(l.split("\t")[1])
                             for l in res.stdout.splitlines() if "\t" in l])

    py_scores = score_all(m, graphs)
    d = np.abs(c_scores - py_scores)
    print(f"parity: n={len(graphs)} H={H} R={R}")
    for i in range(min(len(graphs), 8)):
        print(f"  g{i}: C={c_scores[i]: .6f}  py={py_scores[i]: .6f}  "
              f"|d|={d[i]:.2e}")
    print(f"  max|diff|={d.max():.3e}  mean|diff|={d.mean():.3e}")
    ok = d.max() < 1e-3
    print("PARITY OK" if ok else "PARITY FAIL")
    sys.exit(0 if ok else 1)


def cmd_score(argv):
    m = load_safetensors(argv[0])
    pos, neg, _ = load_graphs([argv[1]], neg_cap=10**9)
    g = pos + neg
    y = [x[2] for x in g]
    s = score_all(m, g)
    print(f"rows={len(g)} pos={sum(y)}  AUC={auc(y, s):.4f}")


def main():
    if len(sys.argv) < 2:
        print(__doc__ if __doc__ else "see header", file=sys.stderr)
        sys.exit(2)
    cmd, rest = sys.argv[1], sys.argv[2:]
    if cmd == "train":
        cmd_train(rest)
    elif cmd == "parity":
        cmd_parity(rest)
    elif cmd == "score":
        cmd_score(rest)
    else:
        print(f"unknown subcommand {cmd}", file=sys.stderr)
        sys.exit(2)


if __name__ == "__main__":
    main()
