#!/usr/bin/env python3
# The M3 trainer (docs/plans/enigma_portfolio.md, sections 3.2 and M3): heads
# for the ENIGMA selection layer (src/atp/guide.c, layout v1, docs/atp/guide.md)
# on py/thvm, grown from topk_net.py's train2 step.
#
# Data: label_join.py's groups_<lane>.npz, one group per dumped pick (its
# candidates in evaluation order, row 0 the engine's next CP).  Inputs are
# layout v1's: x[0..20) as the layer computed them, then the run's phi(p)
# (Phi columns), then the one-hot of its lane over LaneNames (the trained
# lanes in FindProof --list-lanes order), so one head is conditioned on the
# lane.  The score is the layer's
#   s = -kappa x[0] + w2 . relu(W1 z + b1),   z = (x - mu) / sd,
# with w2 = 0 and kappa = 1 at the start (the untrained head is the engine)
# and no output bias: b2 is exported as 0 and never trained, since a
# constant shift cancels in the softmax.  A top-K family (--k K) trains on
# the first K rows of each group, the engine's K best; a lazy family
# (--lazy) on rows with x[0] = 0, which is what the lazy integration feeds.
#
# Loss: listwise within each pick's candidate set, for every group with a
# positive: lse over the candidates minus log sum over the positives of
# w_src exp(s) (w_own, w_cross, w_hind: the label sources of label_join),
# a segment log-softmax of the packed rows by scatter_reduce (amax for the
# shift, sum for the mass), as listwise_net.py's list_loss.  Group weights:
# a run weighs (0.5 + rating) log10(10 + selections) (rating = the TPTP
# Rating; a problem without one, as the synthetic -10 corpus, takes the
# median of the known ratings), normalized to mean 1 over the training
# runs, and its groups share that weight equally.
#
# Splits (amendment A24 item 8): TRAIN and TRAIN-X only, cut by stem
# (stem() for pool names, x_stem() for corpus names, label_join's run_stem):
# VAL when int(sha256(stem + "|val")[:8], 16) % 100 < 15, else fold
# int(sha256("fold|" + stem)[:8], 16) % 5.  Five OOF models (fold k is held
# out) and the full model (every non-VAL run) train with early stopping on
# VAL; the full model is the exported head.
#
# Readout (not a gate: G3.1 is decided at loop 1 on armed-harvest states):
# per lane, on OOF states (each fold's groups under its fold model) and on
# VAL states (the full model), the top-1 proof-CP hit rate of the head's
# pick against the engine's (row 0), over the selections of proved runs
# with a proof CP pending, and on runs with 1000+ selections the
# median-percentile ratio: each proof positive's position in the
# candidate order under the head over its position under the engine, as a
# fraction of the candidates, per run its median, then the median over
# runs.  95% bootstrap intervals resample stems.
#
# py/thvm rules (memory project_atp_enigma_ml): TinyJit for the training
# loop, the default THVM_GC_KB, loss.realize() before opt.step(),
# numpy-seeded initialization, no bias before the softmax, and a "ran no
# dispatch rung" line on stderr is fatal: the trainer runs itself as a
# child and stops (exit 3) on that line.
#
# Usage:
#   guide_net.py train <groups_dir> <out_dir> --list-lanes lanes.txt [--lanes a,b]
#                [--hidden 64] [--k 64 | --lazy [--tau 0]] [--epochs 200]
#                [--patience 10] [--lr 1e-3] [--seed 0] [--max-groups-per-run 256]
#                [--w-own 1] [--w-cross 1] [--w-hind 1] [--folds 5] [--model-id loop0]
#                [--models 0,1,...,full] [--max-hours 0]
#   guide_net.py readout <out_dir>
#   guide_net.py choose <out_dir>... [--tol 0.005]
#   guide_net.py selftest [<out_dir>]
# train writes, per model m (a fold k or full): its head (head.safetensors
# for the full model, which weights_to_c.py build reads, heads/fold<k>
# .safetensors for the folds), models/<m>.json (its record), scores/<m>.npz
# (the rows it evaluated and its float64 scores of them), meta.json and
# train.log (train_<models>.log with --models); when it trained every
# model it runs readout, which merges scores/.  --models trains a subset,
# so the models can run as separate processes on one out_dir.
# --max-hours stops a model at the first epoch end past that many hours
# (0: no budget) with its best VAL epoch so far, and says so.  Each
# lane's x rows are a read-only memmap of its groups file (np.savez
# stores its members), so processes reading one groups_dir share those
# pages; the labels and the group arrays are read into memory.
# choose applies the width rule of amendment A30: of the out_dirs' full
# models, the narrowest hidden whose best VAL loss is within tol (relative)
# of the lowest.
import hashlib
import json
import math
import os
import struct
import subprocess
import sys
import tempfile
import time
import zipfile

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import weights_to_c  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(ROOT, "py"))

N_DYN = 20
OWN, CROSS, HIND = 1, 2, 4
SPLITS = ("train", "TRAIN-X")
BIG = 1e4
R_MAX = 8192        # packed rows per batch
G_MAX = 256         # groups per batch; segment G_MAX holds the padding
FATAL = "ran no dispatch rung"


def die(msg):
    sys.exit("guide_net: " + msg)


def h8(s):
    return int(hashlib.sha256(s.encode()).hexdigest()[:8], 16)


def is_val_stem(stem):
    return h8(stem + "|val") % 100 < 15


def fold_of(stem, k=5):
    return h8("fold|" + stem) % k


# --- data ---------------------------------------------------------------------

def npz_member(path, name):
    """Member <name> of an np.savez file: a read-only memmap of the file
    itself when the member is stored (np.savez stores, savez_compressed
    deflates), else the array read whole."""
    with zipfile.ZipFile(path) as zf:
        info = zf.getinfo(name + ".npy")
    if info.compress_type != zipfile.ZIP_STORED:
        return np.load(path)[name]
    with open(path, "rb") as f:
        f.seek(info.header_offset)
        h = f.read(30)
        if len(h) != 30 or h[:4] != b"PK\x03\x04":
            die(f"{path}: member {name}: no local file header")
        n_name, n_extra = struct.unpack("<HH", h[26:30])
        f.seek(info.header_offset + 30 + n_name + n_extra)
        ver = np.lib.format.read_magic(f)
        if ver == (1, 0):
            shape, fortran, dtype = np.lib.format.read_array_header_1_0(f)
        else:
            shape, fortran, dtype = np.lib.format.read_array_header_2_0(f)
        off = f.tell()
    if dtype.hasobject:
        die(f"{path}: member {name} holds objects")
    if int(np.prod(shape)) == 0:
        return np.zeros(shape, dtype)
    return np.memmap(path, dtype=dtype, mode="r", offset=off, shape=shape,
                     order="F" if fortran else "C")


def src_lut(w_src):
    """log of the largest source weight among a label mask's bits, -BIG
    for a mask without a weighted positive; indexed by lab & 7."""
    lut = np.full(8, -BIG, np.float64)
    for m in range(8):
        ws = [wv for bit, wv in ((OWN, w_src[0]), (CROSS, w_src[1]), (HIND, w_src[2]))
              if m & bit and wv > 0]
        if ws:
            lut[m] = math.log(max(ws))
    return lut


class Data:
    """The groups of the trained lanes: group, run and label arrays
    concatenated in memory, each lane's x rows a memmap of its file."""

    def __init__(self, groups_dir, lanes):
        self.lanes = list(lanes)
        R = {k: [] for k in ("problem", "split", "stem", "mode", "nsel", "rating", "every",
                             "phi", "lane")}
        g_run, g_t, g_n, g_pend, labs = [], [], [], [], []
        self.xl, self.row0 = [], [0]
        n_runs = 0
        for li, lane in enumerate(self.lanes):
            path = os.path.join(groups_dir, f"groups_{lane}.npz")
            if not os.path.exists(path):
                die(f"no {path}")
            z = np.load(path)
            if str(z["lane"]) != lane:
                die(f"{path}: lane {z['lane']}")
            for sp in set(z["run_split"].tolist()):
                if sp not in SPLITS:
                    die(f"{path}: split {sp}: only TRAIN and TRAIN-X are training data (A24)")
            x = npz_member(path, "x")
            if x.dtype != np.float32 or x.ndim != 2 or x.shape[1] != N_DYN:
                die(f"{path}: x is {x.dtype} {x.shape}, not float32 [N, {N_DYN}]")
            off = z["g_off"].astype(np.int64)
            lab = z["lab"].astype(np.uint8)
            if int(off[-1]) != len(x) or len(lab) != len(x):
                die(f"{path}: g_off ends at {int(off[-1])}, x has {len(x)} rows, lab {len(lab)}")
            self.xl.append(x)
            self.row0.append(self.row0[-1] + len(x))
            r = len(z["run_problem"])
            R["problem"] += z["run_problem"].tolist()
            R["split"] += z["run_split"].tolist()
            R["stem"] += z["run_stem"].tolist()
            R["mode"] += z["run_mode"].tolist()
            R["nsel"].append(z["run_nsel"].astype(np.int64))
            R["rating"].append(z["run_rating"].astype(np.float64))
            R["every"].append(z["run_every"].astype(np.int64))
            R["phi"].append(z["run_phi"].astype(np.float32))
            R["lane"].append(np.full(r, li, np.int64))
            g_run.append(z["g_run"].astype(np.int64) + n_runs)
            g_t.append(z["g_t"].astype(np.int64))
            g_n.append(np.diff(off))
            g_pend.append(z["g_pend"].astype(np.int64))
            labs.append(lab)
            n_runs += r
        self.problem = R["problem"]
        self.split = R["split"]
        self.stem = R["stem"]
        self.mode = np.array(R["mode"])
        self.nsel = np.concatenate(R["nsel"]) if R["nsel"] else np.zeros(0, np.int64)
        self.rating = np.concatenate(R["rating"]) if R["rating"] else np.zeros(0)
        self.every = np.concatenate(R["every"]) if R["every"] else np.zeros(0, np.int64)
        phis = [p for p in R["phi"] if len(p)]
        self.n_phi = phis[0].shape[1] if phis else 0
        for p in phis:
            if p.shape[1] != self.n_phi:
                die("lanes disagree on the number of phi(p) columns")
        self.phi = np.concatenate(phis) if phis else np.zeros((0, 0), np.float32)
        self.run_lane = np.concatenate(R["lane"]) if R["lane"] else np.zeros(0, np.int64)
        self.g_run = np.concatenate(g_run) if g_run else np.zeros(0, np.int64)
        self.g_t = np.concatenate(g_t) if g_t else np.zeros(0, np.int64)
        self.g_n = np.concatenate(g_n) if g_n else np.zeros(0, np.int64)
        self.g_pend = np.concatenate(g_pend) if g_pend else np.zeros(0, np.int64)
        self.g_off = np.concatenate([[0], np.cumsum(self.g_n)]).astype(np.int64)
        self.g_lane = self.run_lane[self.g_run]
        self.lab = np.concatenate(labs) if labs else np.zeros(0, np.uint8)
        self.n_rows = self.row0[-1]
        self.val = np.array([is_val_stem(s) for s in self.stem], bool)
        self.n_x = N_DYN + self.n_phi + len(self.lanes)

    def xrows(self, g, k):
        """x[0..20) of group g's first k rows (a view of its lane's file)."""
        li = int(self.g_lane[g])
        a = int(self.g_off[g]) - self.row0[li]
        return self.xl[li][a:a + min(int(self.g_n[g]), k)]

    def run_weights(self, runs):
        """(0.5 + rating) log10(10 + selections) over runs, mean 1."""
        known = self.rating[runs][self.rating[runs] >= 0]
        fill = float(np.median(known)) if len(known) else 0.5
        rt = np.where(self.rating >= 0, self.rating, fill)
        w = (0.5 + rt) * np.log10(10.0 + self.nsel.astype(np.float64))
        out = np.zeros(len(self.rating))
        if len(runs):
            out[runs] = w[runs] / w[runs].mean()
        return out

    def group_pos(self, k, w_src):
        """Per group: whether its first k rows hold a positive the source
        weights count; and the label lookup of the rows' log source
        weights (src_lut)."""
        lut = src_lut(w_src)
        pos = np.zeros(self.n_rows + 1, np.int32)
        np.cumsum((lut > -BIG)[self.lab & 7], dtype=np.int32, out=pos[1:])
        end = self.g_off[:-1] + np.minimum(self.g_n, k)
        has = pos[end] > pos[self.g_off[:-1]]
        return has, lut


def fit_norm(data, groups, k, lazy):
    """mu, sd (float32) of the training rows: the rows of the groups used,
    their first k each; constant columns get sd 1."""
    n_x = data.n_x
    s1 = np.zeros(n_x)
    s2 = np.zeros(n_x)
    n = 0
    lane_rows = np.zeros(len(data.lanes))
    phi_s1 = np.zeros(data.n_phi)
    phi_s2 = np.zeros(data.n_phi)
    rows_per_run = np.zeros(len(data.nsel))
    for g in groups:
        x = data.xrows(g, k).astype(np.float64)
        if lazy:
            x[:, 0] = 0.0
        s1[:N_DYN] += x.sum(0)
        s2[:N_DYN] += (x * x).sum(0)
        n += len(x)
        rows_per_run[data.g_run[g]] += len(x)
    for r in np.nonzero(rows_per_run)[0]:
        c = rows_per_run[r]
        phi_s1 += c * data.phi[r]
        phi_s2 += c * data.phi[r].astype(np.float64) ** 2
        lane_rows[data.run_lane[r]] += c
    s1[N_DYN:N_DYN + data.n_phi] = phi_s1
    s2[N_DYN:N_DYN + data.n_phi] = phi_s2
    s1[N_DYN + data.n_phi:] = lane_rows
    s2[N_DYN + data.n_phi:] = lane_rows
    mu = s1 / max(n, 1)
    var = np.maximum(s2 / max(n, 1) - mu * mu, 0.0)
    sd = np.sqrt(var)
    sd = np.where(sd > 1e-6, sd, 1.0)
    return mu.astype(np.float32), sd.astype(np.float32)


def rows_full(data, g, k, lazy):
    """The full v1 inputs of group g's first k rows (float64)."""
    xg = data.xrows(g, k)
    x = np.zeros((len(xg), data.n_x))
    x[:, :N_DYN] = xg
    if lazy:
        x[:, 0] = 0.0
    r = data.g_run[g]
    x[:, N_DYN:N_DYN + data.n_phi] = data.phi[r]
    x[:, N_DYN + data.n_phi + data.run_lane[r]] = 1.0
    return x


class Packer:
    """Fixed-shape batches of whole groups: z [R_MAX, n_x], x0 [1, R_MAX],
    lw, pm [1, R_MAX], seg [1, R_MAX], sw [1, G_MAX + 1]."""

    def __init__(self, data, mu, sd, k, lazy, lut):
        self.d, self.mu, self.sd, self.k, self.lazy, self.lut = data, mu, sd, k, lazy, lut

    def batches(self, groups, gw):
        out, cur, rows = [], [], 0
        for g in groups:
            n = min(int(self.d.g_n[g]), self.k)
            if cur and (rows + n > R_MAX or len(cur) >= G_MAX):
                out.append(cur)
                cur, rows = [], 0
            cur.append(g)
            rows += n
        if cur:
            out.append(cur)
        return out

    def arrays(self, batch, gw):
        z = np.zeros((R_MAX, self.d.n_x), np.float32)
        x0 = np.zeros((1, R_MAX), np.float32)
        lw = np.full((1, R_MAX), -BIG, np.float32)
        pm = np.full((1, R_MAX), -BIG, np.float32)
        seg = np.full((1, R_MAX), G_MAX, np.int32)
        sw = np.zeros((1, G_MAX + 1), np.float32)
        r = 0
        for i, g in enumerate(batch):
            x = rows_full(self.d, g, self.k, self.lazy)
            n = len(x)
            a = self.d.g_off[g]
            z[r:r + n] = ((x - self.mu) / self.sd).astype(np.float32)
            x0[0, r:r + n] = x[:, 0]
            lw[0, r:r + n] = 0.0
            pm[0, r:r + n] = self.lut[self.d.lab[a:a + n] & 7]
            seg[0, r:r + n] = i
            sw[0, i] = gw[g]
            r += n
        return z, x0, lw, pm, seg, sw

# --- the model (py/thvm) ---------------------------------------------------------

def thvm_api():
    from thvm import Context, Tensor, TinyJit, nn
    return Context, Tensor, TinyJit, nn


class Head:
    def __init__(self, n_x, hid, seed):
        _, Tensor, _, nn = thvm_api()
        rng = np.random.default_rng(seed)
        self.h1 = nn.Linear(n_x, hid)
        self.h1.weight = Tensor((rng.standard_normal((hid, n_x)) / np.sqrt(n_x)).astype(np.float32))
        self.h1.bias = Tensor(np.zeros(hid, np.float32))
        self.out = nn.Linear(hid, 1, bias=False)
        self.out.weight = Tensor(np.zeros((1, hid), np.float32))
        self.kappa = Tensor(np.ones(1, np.float32))
        self.hid = hid

    def params(self):
        return [self.h1.weight, self.h1.bias, self.out.weight, self.kappa]

    def scores(self, z, x0):
        return self.out(self.h1(z).relu()).reshape(1, R_MAX) - self.kappa * x0

    def numpy(self):
        return {"w1": self.h1.weight.numpy().astype(np.float64),
                "b1": self.h1.bias.numpy().astype(np.float64),
                "w2": self.out.weight.numpy().astype(np.float64).reshape(-1),
                "kappa": float(self.kappa.numpy().reshape(-1)[0])}

    def load(self, p):
        _, Tensor, _, _ = thvm_api()
        self.h1.weight.assign(Tensor(p["w1"].astype(np.float32))).realize()
        self.h1.bias.assign(Tensor(p["b1"].astype(np.float32))).realize()
        self.out.weight.assign(Tensor(p["w2"].reshape(1, -1).astype(np.float32))).realize()
        self.kappa.assign(Tensor(np.array([p["kappa"]], np.float32))).realize()


def list_loss(Tensor, s, lw, pm, seg, sw):
    """listwise_net.py's list_loss: per segment b of the packed rows,
    sw[b] (lse over its candidates - lse over its weighted positives),
    each shifted by its detached max."""
    S_, B_ = sw.shape
    X = s + lw
    Xp = s + pm
    m = Tensor.full((S_, B_), -BIG).scatter_reduce(1, seg, X.detach(), "amax")
    mp = Tensor.full((S_, B_), -BIG).scatter_reduce(1, seg, Xp.detach(), "amax")
    den = Tensor.zeros(S_, B_).scatter_reduce(1, seg, (X - m.gather(1, seg)).exp(), "sum")
    num = Tensor.zeros(S_, B_).scatter_reduce(1, seg, (Xp - mp.gather(1, seg)).exp(), "sum")
    return (sw * ((den + 1e-30).log() + m - (num + 1e-30).log() - mp)).sum()


def np_list_loss(s, lw, pm, seg, sw):
    """float64 reference of list_loss."""
    s = s.astype(np.float64)
    tot = 0.0
    for b in range(sw.shape[1]):
        if sw[0, b] == 0:
            continue
        rows = seg[0] == b
        X = s[0, rows] + lw[0, rows]
        Xp = s[0, rows] + pm[0, rows]
        mx, mpx = max(-BIG, X.max()), max(-BIG, Xp.max())
        tot += sw[0, b] * (np.log(np.exp(X - mx).sum() + 1e-30) + mx
                           - np.log(np.exp(Xp - mpx).sum() + 1e-30) - mpx)
    return tot


def np_scores(p, mu, sd, x):
    """The head on full v1 rows in float64: the layer's own formula."""
    zz = (x - mu.astype(np.float64)) / sd.astype(np.float64)
    corr = np.maximum(zz @ p["w1"].T + p["b1"], 0.0) @ p["w2"]
    return -p["kappa"] * x[:, 0] + corr


# --- training ------------------------------------------------------------------

def train_one(data, groups_tr, groups_va, args, seed, log):
    """One model with early stopping on the VAL groups; returns its numpy
    parameters, mu, sd and a record."""
    Context, Tensor, TinyJit, nn = thvm_api()
    k, lazy = args["k"], args["lazy"]
    w_src = (args["w_own"], args["w_cross"], args["w_hind"])
    t_start = time.time()
    has, lut = data.group_pos(k, w_src)
    tr_runs = np.unique(data.g_run[groups_tr])
    rw = data.run_weights(tr_runs)
    groups_tr = groups_tr[has[groups_tr]]
    groups_va = groups_va[has[groups_va]]
    if len(groups_tr) == 0 or len(groups_va) == 0:
        die(f"no training ({len(groups_tr)}) or validation ({len(groups_va)}) group with a positive")
    mu, sd = fit_norm(data, groups_tr, k, lazy)
    pk = Packer(data, mu, sd, k, lazy, lut)
    model = Head(data.n_x, args["hidden"], seed)
    rng = np.random.default_rng(seed)
    with Context(TRAINING=1):
        opt = nn.optim.Adam(model.params(), lr=args["lr"])

        def step(z, x0, lw, pm, seg, sw):
            loss = list_loss(Tensor, model.scores(z, x0), lw, pm, seg, sw)
            opt.zero_grad()
            loss.backward()
            loss.realize()
            opt.step()
            return loss
        jit = TinyJit(step)
    vjit = TinyJit(lambda z, x0, lw, pm, seg, sw:
                   list_loss(Tensor, model.scores(z, x0), lw, pm, seg, sw).realize())
    # VAL: the groups with a positive (at most max_groups_per_run of a
    # run, a fixed sample), the run weight shared.
    cap = args["max_groups_per_run"]
    if cap:
        vr = np.random.default_rng(1234)
        keep = []
        for r in np.unique(data.g_run[groups_va]).tolist():
            gs = groups_va[data.g_run[groups_va] == r]
            keep.append(np.sort(vr.choice(gs, cap, replace=False)) if len(gs) > cap else gs)
        groups_va = np.concatenate(keep)
    va_runs = np.unique(data.g_run[groups_va])
    vrw = data.run_weights(va_runs)
    vcount = np.bincount(data.g_run[groups_va], minlength=len(data.nsel))
    gw_va = np.zeros(len(data.g_n))
    gw_va[groups_va] = vrw[data.g_run[groups_va]] / vcount[data.g_run[groups_va]]
    gw_va[groups_va] /= gw_va[groups_va].sum()
    vb = pk.batches(groups_va, gw_va)

    def tens(arrs):
        return [Tensor(a).realize() for a in arrs]

    # The VAL batches are packed anew at every evaluation: the same groups
    # and weights each time, without holding their tensors between epochs.
    def val_loss():
        return sum(float(vjit(*tens(pk.arrays(b, gw_va))).numpy()) for b in vb)
    best = (val_loss(), -1, model.numpy())
    first_val = best[0]
    log(f"  epoch -1 (the engine: w2 0, kappa 1) val {best[0]:.5f}; train groups "
        f"{len(groups_tr)} of {len(tr_runs)} runs, val groups {len(groups_va)}")
    by_run = {}
    for g in groups_tr.tolist():
        by_run.setdefault(int(data.g_run[g]), []).append(g)
    stop = f"the {args['epochs']}-epoch cap"
    ep_run = 0
    for ep in range(args["epochs"]):
        t0 = time.time()
        ep_run = ep + 1
        pick = []
        for r, gs in by_run.items():
            if cap and len(gs) > cap:
                gs = rng.choice(gs, cap, replace=False).tolist()
            pick += gs
        pick = np.array(pick, np.int64)
        cnt = np.bincount(data.g_run[pick], minlength=len(data.nsel))
        gw = np.zeros(len(data.g_n))
        gw[pick] = rw[data.g_run[pick]] / cnt[data.g_run[pick]]
        pick = pick[rng.permutation(len(pick))]
        bl = pk.batches(pick, gw)
        gw[pick] *= len(bl) / gw[pick].sum()
        tl = 0.0
        with Context(TRAINING=1):
            for b in bl:
                tl += float(jit(*tens(pk.arrays(b, gw))).numpy())
        vl = val_loss()
        log(f"  epoch {ep} train {tl / max(1, len(bl)):.5f} val {vl:.5f} kappa "
            f"{float(model.kappa.numpy().reshape(-1)[0]):.4f} ({len(bl)} batches, "
            f"{time.time() - t0:.1f}s)")
        if vl < best[0] - 1e-7:
            best = (vl, ep, model.numpy())
        elif ep - best[1] >= args["patience"]:
            stop = f"no validation improvement for {args['patience']} epochs"
            break
        if args["max_hours"] > 0 and time.time() - t_start > 3600.0 * args["max_hours"]:
            stop = f"the {args['max_hours']:g}-hour budget"
            break
    p = best[2]
    if p["kappa"] < 0:
        log(f"  kappa {p['kappa']:.4f} < 0 clipped to 0 (the layer requires kappa >= 0)")
        p["kappa"] = 0.0
    rec = {"best_epoch": best[1], "best_val": best[0], "engine_val": first_val, "stop": stop,
           "epochs_run": ep_run, "hours": (time.time() - t_start) / 3600.0,
           "train_groups": int(len(groups_tr)), "val_groups": int(len(groups_va)),
           "train_runs": int(len(tr_runs)), "seed": seed, "hidden": int(args["hidden"])}
    log(f"  best epoch {best[1]} val {best[0]:.5f}; stopped: {stop}")
    return p, mu, sd, rec


def lane_order(listed, lanes):
    for ln in lanes:
        if ln not in listed:
            die(f"lane {ln} is not in the --list-lanes file")
    return sorted(lanes, key=listed.index)


def export_head(path, p, mu, sd, data, args, rec):
    hid, n_x = p["w1"].shape
    mode = "lazy" if args["lazy"] else "topk"
    tau = args["tau"] if args["lazy"] else 0.0
    fams = ",".join(f"{ln}:{i}:{mode}:{args['k']}:{tau:g}:0" for i, ln in enumerate(data.lanes))
    tensors = {
        "mu": ([n_x], [float(v) for v in mu]),
        "sd": ([n_x], [float(v) for v in sd]),
        "w1": ([hid, n_x], [float(v) for v in p["w1"].astype(np.float32).reshape(-1)]),
        "b1": ([hid], [float(v) for v in p["b1"].astype(np.float32)]),
        "w2": ([hid], [float(v) for v in p["w2"].astype(np.float32)]),
        "b2": ([1], [0.0]),
        "kappa": ([1], [float(np.float32(p["kappa"]))]),
    }
    meta = {"Layout": "v1", "Phi": str(data.n_phi), "Lanes": str(len(data.lanes)),
            "LaneNames": ",".join(data.lanes), "Families": fams, "PhiVersion": "1",
            "Trainer": "guide_net.py", "Seed": str(rec["seed"]),
            "BestEpoch": str(rec["best_epoch"]), "ValLoss": "%.6g" % rec["best_val"],
            "ModelId": args["model_id"]}
    weights_to_c.write_safetensors(path, tensors, meta)
    return weights_to_c.load_head(path)


def score_groups(data, groups, p, mu, sd, k, lazy):
    """float64 head scores of the first k rows of each group, in row
    order: (rows index array, scores)."""
    idx, sc = [], []
    for g in groups:
        x = rows_full(data, g, k, lazy)
        a = data.g_off[g]
        idx.append(np.arange(a, a + len(x)))
        sc.append(np_scores(p, mu, sd, x))
    if not idx:
        return np.zeros(0, np.int64), np.zeros(0)
    return np.concatenate(idx), np.concatenate(sc)


def model_names(n_folds):
    return [str(f) for f in range(n_folds)] + ["full"]


def write_json(path, obj):
    tmp = f"{path}.tmp{os.getpid()}"
    with open(tmp, "w") as f:
        json.dump(obj, f, indent=1)
    os.replace(tmp, path)


def cmd_train(argv):
    a = parse_args(argv)
    gdir, out = a["pos"][0], a["pos"][1]
    every = model_names(a["folds"])
    models = a["models"].split(",") if a["models"] else every
    for m in models:
        if m not in every:
            die(f"--models: '{m}' is not one of {','.join(every)}")
    if len(set(models)) != len(models):
        die("--models names a model twice")
    for sub in ("heads", "models", "scores"):
        os.makedirs(os.path.join(out, sub), exist_ok=True)
    tag = "" if models == every else "_" + "_".join(models)
    logf = open(os.path.join(out, f"train{tag}.log"), "w")

    def log(s):
        print(s, flush=True)
        logf.write(s + "\n")
        logf.flush()
    if not a["list_lanes"]:
        die("train needs --list-lanes (FindProof --list-lanes output: LaneNames' order)")
    listed = [ln.strip() for ln in open(a["list_lanes"]) if ln.strip()]
    lanes = a["lanes"].split(",") if a["lanes"] else sorted(
        f[7:-4] for f in os.listdir(gdir) if f.startswith("groups_") and f.endswith(".npz"))
    lanes = lane_order(listed, lanes)
    data = Data(gdir, lanes)
    log(f"guide_net train: lanes {','.join(lanes)}; runs {len(data.nsel)}, groups {len(data.g_n)}, "
        f"rows {data.n_rows}; n_x {data.n_x} (20 + phi {data.n_phi} + lanes {len(lanes)}); "
        f"VAL runs {int(data.val.sum())}; hidden {a['hidden']}; models {','.join(models)}")
    k = a["k"]
    write_json(os.path.join(out, "meta.json"),
               {"groups_dir": os.path.abspath(gdir), "lanes": lanes, "k": k, "lazy": a["lazy"],
                "folds": a["folds"], "hidden": a["hidden"],
                "args": {kk: v for kk, v in a.items() if kk != "pos"}})
    folds = np.array([fold_of(s, a["folds"]) for s in data.stem], np.int64)
    gval = data.val[data.g_run]
    gfold = folds[data.g_run]
    allg = np.arange(len(data.g_n))
    for m in models:
        if m == "full":
            tr = allg[~gval]
            ev = allg[gval]
            seed = a["seed"]
        else:
            f = int(m)
            tr = allg[~gval & (gfold != f)]
            ev = allg[~gval & (gfold == f)]
            seed = a["seed"] + 1 + f
        log(f"model {m}:")
        p, mu, sd, r = train_one(data, tr, allg[gval], a, seed, log)
        path = os.path.join(out, "head.safetensors" if m == "full" else f"heads/fold{m}.safetensors")
        export_head(path, p, mu, sd, data, a, r)
        ri, sc = score_groups(data, ev, p, mu, sd, k, a["lazy"])
        np.savez(os.path.join(out, "scores", f"{m}.npz"), rows=ri, scores=sc)
        write_json(os.path.join(out, "models", f"{m}.json"), r)
    if models == every:
        log("readout:")
        txt = readout_dir(out, data)
        log(txt)
    logf.close()
    return 0


# --- readout ----------------------------------------------------------------------

def boot_ratio(per_run, stems, f, n=2000, seed=0):
    """95% interval of f over runs resampled by stem."""
    if not per_run:
        return (float("nan"), float("nan"))
    by = {}
    for i, s in enumerate(stems):
        by.setdefault(s, []).append(i)
    keys = list(by)
    rng = np.random.default_rng(seed)
    out = []
    for _ in range(n):
        pick = rng.integers(0, len(keys), len(keys))
        idx = [i for j in pick for i in by[keys[j]]]
        v = f(idx)
        if np.isfinite(v):
            out.append(v)
    if not out:
        return (float("nan"), float("nan"))
    return tuple(np.percentile(out, [2.5, 97.5]).tolist())


def readout(data, scores, k):
    lines = ["readout (not a gate; G3.1 is decided at loop 1 on armed-harvest states): per lane, "
             "OOF states under their fold model and VAL states under the full model"]
    for li, lane in enumerate(data.lanes):
        for part in ("OOF", "VAL"):
            hit_e, hit_m, hit_e_any, hit_m_any, med_e, med_m, stems, rstem = {}, {}, {}, {}, {}, {}, [], []
            for g in range(len(data.g_n)):
                r = int(data.g_run[g])
                if data.run_lane[r] != li or (part == "VAL") != bool(data.val[r]):
                    continue
                a = data.g_off[g]
                n = min(int(data.g_n[g]), k)
                sc = scores[a:a + n]
                if not np.all(np.isfinite(sc)):
                    continue
                lab = data.lab[a:a + n]
                pick = int(np.argmax(sc))
                if data.mode[r] == "proof" and data.g_pend[g] > 0:
                    hit_e.setdefault(r, []).append(float(lab[0] & OWN != 0))
                    hit_m.setdefault(r, []).append(float(lab[pick] & OWN != 0))
                if np.any(lab):
                    hit_e_any.setdefault(r, []).append(float(lab[0] != 0))
                    hit_m_any.setdefault(r, []).append(float(lab[pick] != 0))
                pos = np.nonzero(lab & OWN)[0]
                if len(pos) and data.nsel[r] >= 1000:
                    order = np.argsort(-sc, kind="stable")
                    where = np.empty(n, np.int64)
                    where[order] = np.arange(n)
                    med_e.setdefault(r, []).extend((pos / n).tolist())
                    med_m.setdefault(r, []).extend((where[pos] / n).tolist())
            runs = sorted(hit_e)
            if runs:
                he = np.array([np.mean(hit_e[r]) for r in runs])
                hm = np.array([np.mean(hit_m[r]) for r in runs])
                st = [data.stem[r] for r in runs]
                ci = boot_ratio(runs, st, lambda ix: hm[ix].mean() / he[ix].mean()
                                if he[ix].mean() > 0 else float("nan"))
                rho = hm.mean() / he.mean() if he.mean() > 0 else float("nan")
                lines.append(f"  {lane} {part}: top-1 proof-CP hit, head {hm.mean():.4f} engine "
                             f"{he.mean():.4f}, ratio {rho:.3f} CI [{ci[0]:.3f}, {ci[1]:.3f}] "
                             f"({len(runs)} proved runs, {sum(len(hit_e[r]) for r in runs)} selections "
                             f"with a proof CP pending)")
            else:
                lines.append(f"  {lane} {part}: top-1 proof-CP hit: no proved run with a pending proof CP")
            runs_a = sorted(hit_e_any)
            if runs_a:
                he = np.mean([np.mean(hit_e_any[r]) for r in runs_a])
                hm = np.mean([np.mean(hit_m_any[r]) for r in runs_a])
                lines.append(f"    any source (own, cross-lane, hindsight), groups with a positive: "
                             f"head {hm:.4f} engine {he:.4f} ({len(runs_a)} runs)")
            runs_m = sorted(med_e)
            if runs_m:
                me = np.array([np.median(med_e[r]) for r in runs_m])
                mm = np.array([np.median(med_m[r]) for r in runs_m])
                st = [data.stem[r] for r in runs_m]
                ratio = float(np.median(mm) / np.median(me)) if np.median(me) > 0 else float("nan")
                ci = boot_ratio(runs_m, st, lambda ix: np.median(mm[ix]) / np.median(me[ix])
                                if np.median(me[ix]) > 0 else float("nan"))
                lines.append(f"    runs with 1000+ selections: median-percentile ratio {ratio:.3f} CI "
                             f"[{ci[0]:.3f}, {ci[1]:.3f}] (head {np.median(mm):.4f}, engine "
                             f"{np.median(me):.4f}; {len(runs_m)} runs)")
            else:
                lines.append("    runs with 1000+ selections: none with a proof positive among the candidates")
    return "\n".join(lines)


def readout_dir(out, data=None):
    """The readout of a train out_dir: the scored rows of every model
    present (scores/<m>.npz) merged, their records, written to
    readout.txt."""
    meta = json.load(open(os.path.join(out, "meta.json")))
    if data is None:
        data = Data(meta["groups_dir"], meta["lanes"])
    scores = np.full(data.n_rows, np.nan)
    head = [f"models (hidden {meta['hidden']}, k {meta['k']}{', lazy' if meta['lazy'] else ''}):"]
    missing = []
    for m in model_names(meta["folds"]):
        f = os.path.join(out, "scores", f"{m}.npz")
        j = os.path.join(out, "models", f"{m}.json")
        if not (os.path.exists(f) and os.path.exists(j)):
            missing.append(m)
            continue
        z = np.load(f)
        scores[z["rows"]] = z["scores"]
        r = json.load(open(j))
        head.append(f"  {m}: best epoch {r['best_epoch']} of {r['epochs_run']}, VAL loss {r['best_val']:.6f} "
                    f"(engine {r['engine_val']:.6f}), stopped: {r['stop']}; {r['train_groups']} training "
                    f"groups of {r['train_runs']} runs, {r['val_groups']} VAL groups, {r['hours']:.2f} h")
    if missing:
        head.append(f"  missing: {','.join(missing)} (their rows are not read)")
    txt = "\n".join(head) + "\n" + readout(data, scores, meta["k"])
    tmp = os.path.join(out, f"readout.txt.tmp{os.getpid()}")
    with open(tmp, "w") as fr:
        fr.write(txt + "\n")
    os.replace(tmp, os.path.join(out, "readout.txt"))
    return txt


def cmd_readout(argv):
    if len(argv) != 1:
        die("readout needs <out_dir> (a train output)")
    print(readout_dir(argv[0]))
    return 0


def choose_width(rows, tol):
    """Amendment A30's width rule over (hidden, best VAL loss of the full
    model): the narrowest hidden whose loss is at most L* + tol |L*|,
    L* the lowest."""
    best = min(v for _, v in rows)
    bar = best + tol * abs(best)
    return min(h for h, v in rows if v <= bar), bar


def cmd_choose(argv):
    tol, dirs, i = 0.005, [], 0
    while i < len(argv):
        if argv[i] == "--tol" and i + 1 < len(argv):
            tol = float(argv[i + 1])
            i += 2
            continue
        dirs.append(argv[i])
        i += 1
    if not dirs or not (math.isfinite(tol) and tol >= 0.0):
        die("choose needs <out_dir>... [--tol t], t >= 0")
    rows, where = [], {}
    for d in dirs:
        f = os.path.join(d, "models", "full.json")
        if not os.path.exists(f):
            die(f"{d}: no full model (models/full.json)")
        r = json.load(open(f))
        h, v = int(r["hidden"]), float(r["best_val"])
        if h in where:
            die(f"{d} and {where[h]} both hold hidden {h}")
        if not math.isfinite(v):
            die(f"{d}: VAL loss {v}")
        where[h] = d
        rows.append((h, v))
    h, bar = choose_width(rows, tol)
    for hh, v in sorted(rows):
        print(f"hidden {hh}: VAL loss {v:.6f} ({'within' if v <= bar else 'over'} the bar {bar:.6f}) {where[hh]}")
    print(f"chosen hidden {h} {where[h]}")
    return 0


def parse_args(argv):
    a = {"pos": [], "list_lanes": "", "lanes": "", "hidden": 64, "k": 64, "lazy": False, "tau": 0.0,
         "epochs": 200, "patience": 10, "lr": 1e-3, "seed": 0, "max_groups_per_run": 256,
         "w_own": 1.0, "w_cross": 1.0, "w_hind": 1.0, "folds": 5, "model_id": "loop0",
         "models": "", "max_hours": 0.0}
    types = {"hidden": int, "k": int, "tau": float, "epochs": int, "patience": int, "lr": float,
             "seed": int, "max_groups_per_run": int, "w_own": float, "w_cross": float,
             "w_hind": float, "folds": int, "max_hours": float}
    i = 0
    while i < len(argv):
        t = argv[i]
        if t == "--lazy":
            a["lazy"] = True
            i += 1
            continue
        if t.startswith("--"):
            key = t[2:].replace("-", "_")
            if key not in a or i + 1 >= len(argv):
                die(f"unknown option {t}")
            a[key] = types.get(key, str)(argv[i + 1])
            i += 2
            continue
        a["pos"].append(t)
        i += 1
    if not 1 <= a["k"] <= 64:
        die("--k must be in 1..64")
    if not 1 <= a["hidden"] <= 128:
        die("--hidden must be in 1..128")
    if not (math.isfinite(a["max_hours"]) and a["max_hours"] >= 0.0):
        die("--max-hours must be >= 0")
    return a


# --- self-test: synthetic groups -----------------------------------------------

def st_groups(d, lanes, n_stems=24, seed=7):
    """Synthetic groups_<lane>.npz files: each stem has a pool problem (train)
    and its -10 encoding (TRAIN-X); positives carry a planted signal in
    x[5] at random engine ranks, so a head can beat the engine."""
    rng = np.random.default_rng(seed)
    for li, lane in enumerate(lanes):
        R = {k: [] for k in ("problem", "split", "stem", "mode", "nsel", "rating", "every", "phi",
                             "complete", "sig")}
        G = {k: [] for k in ("g_run", "g_t", "g_n", "g_chosen", "g_pend")}
        X, LAB, SEQ, SEL = [], [], [], []
        ri = 0
        for s in range(n_stems):
            stem_ = f"GRP{100 + s:03d}"
            for name, split in ((stem_ + "-1", "train"), (stem_ + "-1-10", "TRAIN-X")):
                if (s + li) % 3 == 2 and split == "TRAIN-X":
                    continue
                proved = rng.random() < 0.8
                nsel = int(rng.choice([300, 2500]))
                R["problem"].append(name)
                R["split"].append(split)
                R["stem"].append(stem_)
                R["mode"].append("proof" if proved else "hindsight")
                R["nsel"].append(nsel)
                R["rating"].append(float(rng.choice([-1.0, rng.random()])))
                R["every"].append(1)
                R["phi"].append(rng.integers(0, 5, 40).astype(np.float32))
                R["complete"].append(True)
                R["sig"].append(s)
                for t in range(1, int(rng.integers(20, 40))):
                    n = int(rng.integers(4, 17))
                    x = rng.standard_normal((n, N_DYN)).astype(np.float32)
                    x[:, 0] = np.log1p(np.arange(n))
                    lab = np.zeros(n, np.uint8)
                    if rng.random() < 0.7:
                        j = int(rng.integers(0, n))
                        lab[j] = OWN if proved else HIND
                        x[j, 5] += 2.5
                    if rng.random() < 0.1:
                        lab[int(rng.integers(0, n))] |= CROSS
                    G["g_run"].append(ri)
                    G["g_t"].append(t)
                    G["g_n"].append(n)
                    G["g_chosen"].append(0)
                    G["g_pend"].append(int((lab & OWN).any()) + int(rng.integers(0, 2)))
                    X.append(x)
                    LAB.append(lab)
                    SEQ.append(np.arange(n, dtype=np.uint32) + 100 * t)
                    SEL.append(np.ones(n, np.uint8))
                ri += 1
        g_n = np.array(G["g_n"], np.int64)
        np.savez(os.path.join(d, f"groups_{lane}.npz"), lane=np.array(lane),
                 run_problem=np.array(R["problem"]), run_split=np.array(R["split"]),
                 run_stem=np.array(R["stem"]), run_mode=np.array(R["mode"]),
                 run_nsel=np.array(R["nsel"], np.int64), run_rating=np.array(R["rating"], np.float32),
                 run_every=np.array(R["every"], np.int32), run_phi=np.stack(R["phi"]),
                 run_sig=np.array(R["sig"], np.uint64), run_complete=np.array(R["complete"]),
                 g_run=np.array(G["g_run"], np.int32), g_t=np.array(G["g_t"], np.int32),
                 g_off=np.concatenate([[0], np.cumsum(g_n)]).astype(np.int64),
                 g_chosen=np.array(G["g_chosen"], np.int16), g_pend=np.array(G["g_pend"], np.int32),
                 x=np.concatenate(X), seq=np.concatenate(SEQ), lab=np.concatenate(LAB),
                 sel=np.concatenate(SEL))


def selftest(argv):
    fails = []

    def check(cond, what):
        if not cond:
            fails.append(what)
        print(("ok   " if cond else "FAIL ") + what, flush=True)

    out = argv[0] if argv else tempfile.mkdtemp(prefix="guide_net_st_")
    gdir = os.path.join(out, "groups")
    os.makedirs(gdir, exist_ok=True)
    lanes = ["tgt-sel", "encompass"]
    listed = ["wm", "tgt-sel", "otter-lpo", "encompass", "bare"]
    st_groups(gdir, lanes)
    with open(os.path.join(out, "lanes.txt"), "w") as f:
        f.write("\n".join(listed) + "\n")
    data = Data(gdir, lane_order(listed, lanes))
    check(data.lanes == ["tgt-sel", "encompass"], "LaneNames follow --list-lanes order")
    check(data.n_x == N_DYN + 40 + 2, "n_x = 20 + phi 40 + 2 lanes")
    # A stem's pool and -10 runs share their VAL flag and fold.
    by = {}
    for i, s in enumerate(data.stem):
        by.setdefault(s, set()).add((bool(data.val[i]), fold_of(s)))
    check(all(len(v) == 1 for v in by.values()), "a stem's runs share VAL and fold")
    check(0 < int(data.val.sum()) < len(data.val), "VAL is a proper slice")
    # The loss against float64 numpy on a packed batch.
    Context, Tensor, TinyJit, nn = thvm_api()
    # x is a memmap of each lane's file, equal to the member np.load reads.
    same_x = all(isinstance(data.xl[li], np.memmap) and
                 np.array_equal(np.asarray(data.xl[li]), np.load(os.path.join(gdir, f"groups_{ln}.npz"))["x"])
                 for li, ln in enumerate(data.lanes))
    check(same_x, "x rows: a memmap of each groups file, equal to np.load's")
    zc = os.path.join(out, "compressed.npz")
    np.savez_compressed(zc, x=np.arange(6, dtype=np.float32).reshape(3, 2))
    check(np.array_equal(npz_member(zc, "x"), np.arange(6, dtype=np.float32).reshape(3, 2)),
          "a deflated member is read whole")
    # group_pos against a per-group scan, at k = 3 and k = 64.
    ok_pos = True
    for kk, ws in ((3, (1.0, 0.0, 1.0)), (64, (1.0, 1.0, 1.0))):
        hs, lut_ = data.group_pos(kk, ws)
        for g in range(len(data.g_n)):
            a0 = data.g_off[g]
            lab_g = data.lab[a0:a0 + min(int(data.g_n[g]), kk)]
            want = any((ws[0] > 0 and v & OWN) or (ws[1] > 0 and v & CROSS) or (ws[2] > 0 and v & HIND)
                       for v in lab_g.tolist())
            ok_pos = ok_pos and bool(hs[g]) == bool(want)
        ok_pos = ok_pos and lut_[OWN | HIND] == 0.0 and lut_[0] == -BIG and lut_[CROSS] == (-BIG if ws[1] == 0 else 0.0)
    check(ok_pos, "group_pos: the groups with a counted positive in their first k rows")
    has, lut = data.group_pos(64, (1.0, 1.0, 1.0))
    groups = np.nonzero(has & ~data.val[data.g_run])[0]
    mu, sd = fit_norm(data, groups, 64, False)
    check(bool(np.all(sd > 0)) and mu.dtype == np.float32, "normalization: sd > 0, float32")
    pk = Packer(data, mu, sd, 64, False, lut)
    gw = np.zeros(len(data.g_n))
    gw[groups] = 1.0 / len(groups)
    b0 = pk.batches(groups, gw)[0]
    arrs = pk.arrays(b0, gw)
    model = Head(data.n_x, 16, 3)
    rng = np.random.default_rng(5)
    p = model.numpy()
    p["w2"] = rng.uniform(-0.5, 0.5, 16)
    model.load(p)
    s_t = model.scores(Tensor(arrs[0]), Tensor(arrs[1]))
    loss = float(list_loss(Tensor, s_t, *[Tensor(x) for x in arrs[2:]]).numpy())
    s_np = s_t.numpy().astype(np.float64)
    ref = np_list_loss(s_np, *arrs[2:])
    check(abs(loss - ref) <= 1e-4 * max(1.0, abs(ref)), f"list_loss {loss:.6f} vs numpy {ref:.6f}")
    # The py/thvm scores against the float64 head on the same rows.
    rows = []
    r = 0
    for g in b0:
        x = rows_full(data, g, 64, False)
        rows.append(x)
        r += len(x)
    xf = np.concatenate(rows)
    d64 = float(np.max(np.abs(np_scores(p, mu, sd, xf) - s_np[0, :r])))
    check(d64 < 1e-3, f"py/thvm scores vs the float64 head: max |diff| {d64:.2e}")
    # The untrained head (w2 = 0, kappa 1) picks the engine's row 0.
    p0 = Head(data.n_x, 16, 3).numpy()
    sc = np.full(data.n_rows, np.nan)
    ri, s0 = score_groups(data, np.arange(len(data.g_n)), p0, mu, sd, 64, False)
    sc[ri] = s0
    same = all(int(np.argmax(sc[data.g_off[g]:data.g_off[g + 1]])) == 0 for g in range(len(data.g_n)))
    check(same, "the untrained head picks row 0 at every group (the engine)")
    txt0 = readout(data, sc, 64)
    check("ratio 1.000" in txt0, "the untrained head's readout ratio is 1")
    # Training: TinyJit, early stopping; the head beats the engine on VAL.
    a = parse_args([gdir, out, "--list-lanes", os.path.join(out, "lanes.txt"), "--hidden", "16",
                    "--epochs", "40", "--patience", "6", "--lr", "3e-3", "--folds", "2",
                    "--max-groups-per-run", "64"])
    logs = []
    pt, mut, sdt, rec = train_one(data, np.arange(len(data.g_n))[~data.val[data.g_run]],
                                  np.arange(len(data.g_n))[data.val[data.g_run]], a, 11, logs.append)
    print("\n".join(logs))
    first = float(logs[0].split(" val ")[1].split(";")[0])
    check(rec["best_val"] < 0.9 * first, f"training lowers the VAL loss ({first:.4f} -> {rec['best_val']:.4f})")
    check(rec["best_epoch"] >= 0, "a best epoch was found")
    vg = np.nonzero(data.val[data.g_run])[0]
    ri, sv = score_groups(data, vg, pt, mut, sdt, 64, False)
    sc = np.full(data.n_rows, np.nan)
    sc[ri] = sv
    hm = he = n = 0
    for g in vg:
        a0, a1 = data.g_off[g], data.g_off[g + 1]
        lab = data.lab[a0:a1]
        if not (lab & OWN).any():
            continue
        n += 1
        he += int(lab[0] & OWN != 0)
        hm += int(lab[int(np.argmax(sc[a0:a1]))] & OWN != 0)
    check(n > 0 and hm > 1.5 * max(he, 1), f"VAL top-1 own hit: head {hm} vs engine {he} of {n}")
    # Export and read back through the generator; parity of the formula.
    path = os.path.join(out, "head.safetensors")
    h = export_head(path, pt, mut, sdt, data, a, rec)
    check(h["layout"] == "v1" and h["n_phi"] == 40 and h["n_lane"] == 2 and h["n_x"] == data.n_x,
          "export: layout v1, Phi 40, Lanes 2")
    check(h["lanes"] == ["tgt-sel", "encompass"], "export: LaneNames")
    check(h["b2"] == 0.0 and h["kappa"] >= 0.0, "export: b2 0, kappa >= 0")
    check([f["name"] for f in h["fam"]] == ["tgt-sel", "encompass"] and
          all(f["mode"] == "topk" and f["k"] == 64 for f in h["fam"]), "export: one top-64 family per lane")
    hp = {"w1": np.array(h["w1"], np.float64).reshape(h["hid"], h["n_x"]),
          "b1": np.array(h["b1"], np.float64), "w2": np.array(h["w2"], np.float64).reshape(-1),
          "kappa": float(h["kappa"])}
    ref = np_scores(hp, np.array(h["mu"]), np.array(h["sd"]), xf)
    mine = np_scores({k_: (v.astype(np.float32).astype(np.float64) if isinstance(v, np.ndarray)
                           else float(np.float32(v))) for k_, v in pt.items()}, mut, sdt, xf)
    dd = float(np.max(np.abs(ref - mine)))
    check(dd < 1e-9, f"exported head vs the trainer's float32 weights: max |diff| {dd:.2e}")
    # The whole train command, two folds, on the same groups.
    rc = cmd_train([gdir, os.path.join(out, "run"), "--list-lanes", os.path.join(out, "lanes.txt"),
                    "--hidden", "8", "--epochs", "6", "--patience", "3", "--folds", "2",
                    "--max-groups-per-run", "32"])
    check(rc == 0 and os.path.exists(os.path.join(out, "run", "head.safetensors")) and
          os.path.exists(os.path.join(out, "run", "heads", "fold1.safetensors")), "train writes the heads")
    nsc = sum(len(np.load(os.path.join(out, "run", "scores", f"{m}.npz"))["rows"]) for m in ("0", "1", "full"))
    check(nsc > 0, "train scores OOF and VAL rows")
    rtxt = open(os.path.join(out, "run", "readout.txt")).read()
    check("tgt-sel OOF" in rtxt and "encompass VAL" in rtxt and "missing" not in rtxt,
          "the readout covers each lane and part")
    # The same models trained one subset at a time (as separate processes
    # would) give the same heads and readout.
    base = [gdir, os.path.join(out, "run2"), "--list-lanes", os.path.join(out, "lanes.txt"),
            "--hidden", "8", "--epochs", "6", "--patience", "3", "--folds", "2",
            "--max-groups-per-run", "32"]
    rc1 = cmd_train(base + ["--models", "1"])
    part = readout_dir(os.path.join(out, "run2"))
    rc2 = cmd_train(base + ["--models", "full,0"])
    check(rc1 == 0 and rc2 == 0 and "missing: 0,full" in part, "--models trains a subset; the readout names the missing")
    dmax = 0.0
    for hp in ("head.safetensors", "heads/fold0.safetensors", "heads/fold1.safetensors"):
        h1 = weights_to_c.load_head(os.path.join(out, "run", hp))
        h2 = weights_to_c.load_head(os.path.join(out, "run2", hp))
        for nm in ("mu", "sd", "w1", "b1", "w2"):
            dmax = max(dmax, float(np.max(np.abs(np.array(h1[nm]) - np.array(h2[nm])))))
        dmax = max(dmax, abs(h1["kappa"] - h2["kappa"]))
    check(dmax <= 1e-5, f"heads trained per subset equal the all-in-one heads: max |diff| {dmax:.2e}")
    r2 = readout_dir(os.path.join(out, "run2"))
    check("tgt-sel OOF" in r2 and "encompass VAL" in r2 and "missing" not in r2,
          "the merged readout of the subsets")
    # --max-hours stops at the first epoch end past the budget.
    rc3 = cmd_train(base[:1] + [os.path.join(out, "run3")] + base[2:] + ["--models", "full", "--max-hours", "1e-9"])
    r3 = json.load(open(os.path.join(out, "run3", "models", "full.json")))
    check(rc3 == 0 and r3["epochs_run"] == 1 and r3["stop"] == "the 1e-09-hour budget",
          f"--max-hours: {r3['epochs_run']} epoch, stopped: {r3['stop']}")
    # The width rule (amendment A30).
    check(choose_width([(32, 2.000), (64, 1.995), (128, 1.990)], 0.005)[0] == 64 and
          choose_width([(32, 1.99), (64, 1.98), (128, 1.80)], 0.005)[0] == 128 and
          choose_width([(128, 1.0), (32, 1.0), (64, 1.0)], 0.005)[0] == 32 and
          choose_width([(32, 1.0049), (64, 1.0)], 0.005)[0] == 32 and
          choose_width([(32, 1.0051), (64, 1.0)], 0.005)[0] == 64,
          "choose: the narrowest width within 0.5% of the lowest VAL loss")
    # The split rule.
    bad = os.path.join(out, "bad")
    os.makedirs(bad, exist_ok=True)
    z = dict(np.load(os.path.join(gdir, "groups_tgt-sel.npz")))
    z["run_split"] = np.array(["heldout"] + z["run_split"].tolist()[1:])
    np.savez(os.path.join(bad, "groups_tgt-sel.npz"), **z)
    try:
        Data(bad, ["tgt-sel"])
        check(False, "a heldout run must be refused")
    except SystemExit as e:
        check("only TRAIN and TRAIN-X" in str(e), "a heldout run is refused")
    print("guide_net selftest " + ("ok" if not fails else f"FAILED: {len(fails)}"))
    return 1 if fails else 0


# --- entry: the child guard -------------------------------------------------------

def child_main(argv):
    cmd, rest = argv[0], argv[1:]
    if cmd == "train":
        return cmd_train(rest)
    if cmd == "readout":
        return cmd_readout(rest)
    if cmd == "choose":
        return cmd_choose(rest)
    if cmd == "selftest":
        return selftest(rest)
    die(f"unknown command {cmd}")


def main():
    if len(sys.argv) < 2:
        die("usage: guide_net.py train|readout|choose|selftest ...")
    if sys.argv[1] == "--child":
        return child_main(sys.argv[2:])
    # py/thvm prints "... ran no dispatch rung ..." when a kernel was
    # stripped: that run trained wrong, so the line is fatal.
    p = subprocess.Popen([sys.executable, os.path.abspath(__file__), "--child"] + sys.argv[1:],
                         stderr=subprocess.PIPE, text=True, errors="replace")
    fatal = False
    for line in p.stderr:
        sys.stderr.write(line)
        if FATAL in line:
            fatal = True
            p.kill()
            break
    rc = p.wait()
    if fatal:
        sys.stderr.write(f"guide_net: FATAL: '{FATAL}' on stderr; the run is void\n")
        return 3
    return rc


if __name__ == "__main__":
    sys.exit(main())
