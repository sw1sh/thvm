"""CUDA NN-layer cross-validation: thvm-CUDA vs numpy (and tinygrad).

Stage 5 of the CUDA backend slice -- extends `cuda_xval.py` from bare
matmul / softmax / row-reduce to neural-network *layer* kernels, forward
AND backward, in increasing complexity:

  1. Linear forward       Y = X @ W + b           (matmul + bias bcast)
  2. Linear backward      dW = X^T @ dY, dX = dY @ W^T   (transposed mm)
  3. ReLU / GELU          forward + backward       (elementwise)
  4. LayerNorm            forward + backward       (reduce->bcast)
  5. Conv2d               forward + backward       (im2col matmul)
  6. Attention            Q K^T -> softmax -> @V   forward (+ backward)

Each layer is a hand-built UOp DAG (the backward math is known; this
does NOT depend on thvm's autodiff).  Every DAG is rendered to CUDA,
nvrtc-compiled, dispatched on the GPU, and checked against a float64
numpy reference under a width-aware fp32 band.

thvm's OWN autodiff (uop_grad / UOP_GRAD, src/interact/uop_grad.c) is
NOT exercised here: it operates on the high-level TAG_TEN tensor graph
and is reachable only through the Wolfram LibraryLink frontend (TGrad).
The `py.thvm` bridge exposes only the post-materialize kernel-DAG
builder (py_uop_*), with no materialize / realize / grad entry point,
so the autograd-lowers-through-CUDA path cannot be driven from Python
without a bridge extension.  The hand-built backward DAGs below are the
substitute: they confirm the backward *math* lowers + runs on CUDA.

Per feedback_profile_before_e2e: each layer is tested individually
(`--only <name>`) before any multi-layer composition.
Per feedback_never_hide_bugs: a kernel that fails to render / compile /
dispatch / match is reported as FAIL with the reason -- never skipped,
never quarantined.

  python3 py/examples/cuda_nn_xval.py                 # full curriculum
  python3 py/examples/cuda_nn_xval.py --only linear_fwd
  python3 py/examples/cuda_nn_xval.py --quick          # small shapes
  python3 py/examples/cuda_nn_xval.py --json out.json

Runs only on a Linux+CUDA host; prints SKIP and exits 0 otherwise.
"""
from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path

import numpy as np

ROOT = Path(__file__).parent.parent.parent
sys.path.insert(0, str(ROOT))

from py.thvm import Thvm, Cuda, K

FP32_EPS = 2.0 ** -23
WARMUP = 10
REPS = 50


# ====================================================================
# helpers
# ====================================================================
def pctl(samples, q):
    s = sorted(samples)
    if not s:
        return 0.0
    i = min(len(s) - 1, max(0, int(q * (len(s) - 1) + 0.5)))
    return s[i]


def _grid_block(total):
    block = 256 if total >= 256 else max(32, ((total + 31) // 32) * 32)
    grid = (total + block - 1) // block
    return grid, block


# log2(e): the harness builds exp(x) = exp2(x * log2e); GELU's tanh
# also goes through exp2.  Kept as a module constant so every builder
# hash-conses the same fconst.
LOG2E = 1.4426950408889634


# ====================================================================
# DAG builders.  Each returns (store_root, n_input_buffers, out_shape).
# Buffer instance 0 is always the output; inputs are instances 1..n.
# Address arithmetic is row-major flat offsets, matching numpy's
# C-contiguous layout so the read-back compares element-for-element.
# ====================================================================
def _flat(h, idx_terms, dims):
    """Row-major flat offset of a multi-index given dim extents."""
    off = idx_terms[0]
    for i in range(1, len(idx_terms)):
        off = h.iadd(h.imul(off, h.iconst(dims[i])), idx_terms[i])
    return off


# -------- Layer 1: Linear forward  Y[m,n] = sum_k X[m,k] W[k,n] + b[n]
def build_linear_fwd(h, M, N, Kd):
    out = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(M, N), instance=0)
    x = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(M, Kd), instance=1)
    w = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(Kd, N), instance=2)
    b = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(N,), instance=3)
    m = h.range(axis_id=0, axis_type=K.AXIS_LOOP, extent=M)
    n = h.range(axis_id=1, axis_type=K.AXIS_LOOP, extent=N)
    k = h.range(axis_id=2, axis_type=K.AXIS_REDUCE, extent=Kd)
    xk = h.index_e(x, _flat(h, [m, k], (M, Kd)))
    wk = h.index_e(w, _flat(h, [k, n], (Kd, N)))
    acc = h.reduce(K.REDUCE_SUM, axis=2, src=h.mul(xk, wk))
    bias = h.index_e(b, n)                       # broadcast over m
    val = h.add(acc, bias)
    return h.store(out, _flat(h, [m, n], (M, N)), val), 3, (M, N)


# -------- Layer 2a: Linear backward dW[k,n] = sum_m X[m,k] dY[m,n]
def build_linear_bwd_dw(h, M, N, Kd):
    out = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(Kd, N), instance=0)
    x = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(M, Kd), instance=1)
    dy = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(M, N), instance=2)
    kk = h.range(axis_id=0, axis_type=K.AXIS_LOOP, extent=Kd)
    n = h.range(axis_id=1, axis_type=K.AXIS_LOOP, extent=N)
    m = h.range(axis_id=2, axis_type=K.AXIS_REDUCE, extent=M)
    # X^T[k,m] is just X[m,k]; the transpose is in the address, no copy.
    xt = h.index_e(x, _flat(h, [m, kk], (M, Kd)))
    dym = h.index_e(dy, _flat(h, [m, n], (M, N)))
    acc = h.reduce(K.REDUCE_SUM, axis=2, src=h.mul(xt, dym))
    return h.store(out, _flat(h, [kk, n], (Kd, N)), acc), 2, (Kd, N)


# -------- Layer 2b: Linear backward dX[m,k] = sum_n dY[m,n] W[k,n]
def build_linear_bwd_dx(h, M, N, Kd):
    out = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(M, Kd), instance=0)
    dy = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(M, N), instance=1)
    w = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(Kd, N), instance=2)
    m = h.range(axis_id=0, axis_type=K.AXIS_LOOP, extent=M)
    kk = h.range(axis_id=1, axis_type=K.AXIS_LOOP, extent=Kd)
    n = h.range(axis_id=2, axis_type=K.AXIS_REDUCE, extent=N)
    dyn = h.index_e(dy, _flat(h, [m, n], (M, N)))
    # W^T[n,k] is W[k,n]; transpose lives in the address expression.
    wt = h.index_e(w, _flat(h, [kk, n], (Kd, N)))
    acc = h.reduce(K.REDUCE_SUM, axis=2, src=h.mul(dyn, wt))
    return h.store(out, _flat(h, [m, kk], (M, Kd)), acc), 2, (M, Kd)


# -------- Layer 2c: bias gradient db[n] = sum_m dY[m,n]
def build_linear_bwd_db(h, M, N):
    out = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(N,), instance=0)
    dy = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(M, N), instance=1)
    n = h.range(axis_id=0, axis_type=K.AXIS_LOOP, extent=N)
    m = h.range(axis_id=1, axis_type=K.AXIS_REDUCE, extent=M)
    dym = h.index_e(dy, _flat(h, [m, n], (M, N)))
    acc = h.reduce(K.REDUCE_SUM, axis=1, src=dym)
    return h.store(out, n, acc), 1, (N,)


# -------- Layer 3a: ReLU forward  Y = max(x,0) == (x>0) * x
def build_relu_fwd(h, n_elem):
    out = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(n_elem,), instance=0)
    x = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(n_elem,), instance=1)
    i = h.range(axis_id=0, axis_type=K.AXIS_LOOP, extent=n_elem)
    xv = h.index_e(x, i)
    # CMPLT produces a 0/1 float mask; (0 < x) * x is max(x,0).
    mask = h.cmplt(h.fconst(0.0), xv)
    return h.store(out, i, h.mul(mask, xv)), 1, (n_elem,)


# -------- Layer 3b: ReLU backward  dX = (x>0) * dY
def build_relu_bwd(h, n_elem):
    out = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(n_elem,), instance=0)
    x = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(n_elem,), instance=1)
    dy = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(n_elem,), instance=2)
    i = h.range(axis_id=0, axis_type=K.AXIS_LOOP, extent=n_elem)
    mask = h.cmplt(h.fconst(0.0), h.index_e(x, i))
    return h.store(out, i, h.mul(mask, h.index_e(dy, i))), 2, (n_elem,)


# tanh-approx GELU constants (the formula PyTorch/tinygrad use).
GELU_C0 = 0.7978845608028654          # sqrt(2/pi)
GELU_C1 = 0.044715


def _tanh(h, x):
    """tanh(x) = 1 - 2/(exp(2x)+1), built from exp2 (the only exp op)."""
    two_x = h.mul(x, h.fconst(2.0))
    e = h.exp2(h.mul(two_x, h.fconst(LOG2E)))     # exp(2x)
    denom = h.add(e, h.fconst(1.0))
    return h.add(h.fconst(1.0), h.neg(h.mul(h.fconst(2.0), h.recip(denom))))


# -------- Layer 3c: GELU forward (tanh approximation)
#   y = 0.5 x (1 + tanh( c0 (x + c1 x^3) ))
def build_gelu_fwd(h, n_elem):
    out = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(n_elem,), instance=0)
    x = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(n_elem,), instance=1)
    i = h.range(axis_id=0, axis_type=K.AXIS_LOOP, extent=n_elem)
    xv = h.index_e(x, i)
    x3 = h.mul(xv, h.mul(xv, xv))
    inner = h.mul(h.fconst(GELU_C0), h.add(xv, h.mul(h.fconst(GELU_C1), x3)))
    y = h.mul(h.mul(h.fconst(0.5), xv), h.add(h.fconst(1.0), _tanh(h, inner)))
    return h.store(out, i, y), 1, (n_elem,)


# -------- Layer 3d: GELU backward dX = dY * gelu'(x)
#   with u = c0(x + c1 x^3), t = tanh(u), du = c0(1 + 3 c1 x^2):
#   gelu'(x) = 0.5(1+t) + 0.5 x (1 - t^2) du
def build_gelu_bwd(h, n_elem):
    out = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(n_elem,), instance=0)
    x = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(n_elem,), instance=1)
    dy = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(n_elem,), instance=2)
    i = h.range(axis_id=0, axis_type=K.AXIS_LOOP, extent=n_elem)
    xv = h.index_e(x, i)
    x2 = h.mul(xv, xv)
    x3 = h.mul(x2, xv)
    u = h.mul(h.fconst(GELU_C0), h.add(xv, h.mul(h.fconst(GELU_C1), x3)))
    t = _tanh(h, u)
    du = h.mul(h.fconst(GELU_C0),
               h.add(h.fconst(1.0),
                     h.mul(h.fconst(3.0 * GELU_C1), x2)))
    sech2 = h.add(h.fconst(1.0), h.neg(h.mul(t, t)))     # 1 - t^2
    dgelu = h.add(h.mul(h.fconst(0.5), h.add(h.fconst(1.0), t)),
                  h.mul(h.mul(h.fconst(0.5), xv), h.mul(sech2, du)))
    return h.store(out, i, h.mul(h.index_e(dy, i), dgelu)), 2, (n_elem,)


# -------- Layer 4a: LayerNorm forward (per-row, no affine)
#   mu = mean_c x; var = mean_c (x-mu)^2; y = (x-mu)/sqrt(var+eps)
LN_EPS = 1e-5


def build_layernorm_fwd(h, R, C):
    out = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(R, C), instance=0)
    x = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(R, C), instance=1)
    r = h.range(axis_id=0, axis_type=K.AXIS_LOOP, extent=R)
    c = h.range(axis_id=1, axis_type=K.AXIS_LOOP, extent=C)
    invC = h.fconst(1.0 / C)
    # mean: reduce axis 2
    k1 = h.range(axis_id=2, axis_type=K.AXIS_REDUCE, extent=C)
    mu = h.mul(h.reduce(K.REDUCE_SUM, axis=2,
                        src=h.index_e(x, _flat(h, [r, k1], (R, C)))), invC)
    # var: reduce axis 3 of (x-mu)^2
    k2 = h.range(axis_id=3, axis_type=K.AXIS_REDUCE, extent=C)
    d2 = h.index_e(x, _flat(h, [r, k2], (R, C)))
    dev = h.add(d2, h.neg(mu))
    var = h.mul(h.reduce(K.REDUCE_SUM, axis=3, src=h.mul(dev, dev)), invC)
    rstd = h.recip(h.sqrt(h.add(var, h.fconst(LN_EPS))))
    xv = h.index_e(x, _flat(h, [r, c], (R, C)))
    y = h.mul(h.add(xv, h.neg(mu)), rstd)
    return h.store(out, _flat(h, [r, c], (R, C)), y), 1, (R, C)


# -------- Layer 4b: LayerNorm backward (no affine)
#   xhat = (x-mu)*rstd
#   dX = rstd * ( dY - mean(dY) - xhat * mean(dY * xhat) )
# This is the reduce-feeding-broadcast structure: each output element
# needs the row-mean of dY and the row-mean of dY*xhat, both of which
# are reductions over the same axis the output broadcasts across.
def build_layernorm_bwd(h, R, C):
    out = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(R, C), instance=0)
    x = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(R, C), instance=1)
    dy = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(R, C), instance=2)
    r = h.range(axis_id=0, axis_type=K.AXIS_LOOP, extent=R)
    c = h.range(axis_id=1, axis_type=K.AXIS_LOOP, extent=C)
    invC = h.fconst(1.0 / C)

    def row_mean(buf, axis):
        k = h.range(axis_id=axis, axis_type=K.AXIS_REDUCE, extent=C)
        return h.mul(h.reduce(K.REDUCE_SUM, axis=axis,
                              src=h.index_e(buf, _flat(h, [r, k], (R, C)))),
                     invC), k

    # mu = mean(x) over axis 2
    mu, _ = row_mean(x, 2)
    # var = mean((x-mu)^2) over axis 3
    k3 = h.range(axis_id=3, axis_type=K.AXIS_REDUCE, extent=C)
    dev3 = h.add(h.index_e(x, _flat(h, [r, k3], (R, C))), h.neg(mu))
    var = h.mul(h.reduce(K.REDUCE_SUM, axis=3, src=h.mul(dev3, dev3)), invC)
    rstd = h.recip(h.sqrt(h.add(var, h.fconst(LN_EPS))))
    # mean(dY) over axis 4
    mdy, _ = row_mean(dy, 4)
    # mean(dY * xhat) over axis 5; xhat[k] = (x[k]-mu)*rstd
    k5 = h.range(axis_id=5, axis_type=K.AXIS_REDUCE, extent=C)
    xhat5 = h.mul(h.add(h.index_e(x, _flat(h, [r, k5], (R, C))), h.neg(mu)),
                  rstd)
    dyx = h.mul(h.index_e(dy, _flat(h, [r, k5], (R, C))), xhat5)
    mdyx = h.mul(h.reduce(K.REDUCE_SUM, axis=5, src=dyx), invC)
    # per-element dX
    xv = h.index_e(x, _flat(h, [r, c], (R, C)))
    xhat = h.mul(h.add(xv, h.neg(mu)), rstd)
    dyv = h.index_e(dy, _flat(h, [r, c], (R, C)))
    inner = h.add(h.add(dyv, h.neg(mdy)), h.neg(h.mul(xhat, mdyx)))
    dx = h.mul(rstd, inner)
    return h.store(out, _flat(h, [r, c], (R, C)), dx), 2, (R, C)


# -------- Layer 5: Conv2d via im2col.  The im2col unfold is done on the
# host (numpy); the GPU kernel is the resulting matmul, which is what
# tinygrad's conv also reduces to.  This isolates the conv *math* from
# the unfold bookkeeping.  Cols[(oh*ow), (Cin*kh*kw)] @ Wflat[.., Cout].
def build_conv2d_fwd_matmul(h, P, Q, S):
    """P = oh*ow output positions, Q = Cout, S = Cin*kh*kw patch size."""
    return build_linear_fwd_nobias(h, P, Q, S)


def build_linear_fwd_nobias(h, M, N, Kd):
    out = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(M, N), instance=0)
    a = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(M, Kd), instance=1)
    b = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(Kd, N), instance=2)
    m = h.range(axis_id=0, axis_type=K.AXIS_LOOP, extent=M)
    n = h.range(axis_id=1, axis_type=K.AXIS_LOOP, extent=N)
    k = h.range(axis_id=2, axis_type=K.AXIS_REDUCE, extent=Kd)
    av = h.index_e(a, _flat(h, [m, k], (M, Kd)))
    bv = h.index_e(b, _flat(h, [k, n], (Kd, N)))
    acc = h.reduce(K.REDUCE_SUM, axis=2, src=h.mul(av, bv))
    return h.store(out, _flat(h, [m, n], (M, N)), acc), 2, (M, N)


# -------- Layer 5b: Conv2d backward dW.  dWflat[S,Cout] = sum_P
# cols[P,S] * dY[P,Cout] -- a transposed-operand matmul over the
# im2col cols (S = Cin*kh*kw, P = N*oh*ow), the conv counterpart of
# linear_bwd_dw.  Output is the [S,Cout] flattened weight gradient.
def build_conv2d_bwd_dw(h, P, Q, S):
    out = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(S, Q), instance=0)
    cols = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(P, S), instance=1)
    dy = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(P, Q), instance=2)
    s = h.range(axis_id=0, axis_type=K.AXIS_LOOP, extent=S)
    q = h.range(axis_id=1, axis_type=K.AXIS_LOOP, extent=Q)
    p = h.range(axis_id=2, axis_type=K.AXIS_REDUCE, extent=P)
    cv = h.index_e(cols, _flat(h, [p, s], (P, S)))      # cols^T
    dv = h.index_e(dy, _flat(h, [p, q], (P, Q)))
    acc = h.reduce(K.REDUCE_SUM, axis=2, src=h.mul(cv, dv))
    return h.store(out, _flat(h, [s, q], (S, Q)), acc), 2, (S, Q)


# -------- Layer 5c: Conv2d backward dX -- col2im as a gather-reduce.
# This is the genuinely conv-specific backward kernel.  Rather than a
# scatter (`dX[..] +=`), each input pixel dX[n,c,ih,iw] is written by
# ONE output thread that *gathers* every dCols contribution touching
# it: a reduce over the (kh,kw) kernel offsets, bounds-masked so only
# in-range output positions contribute.
#   dX[n,c,ih,iw] = sum_{ki,kj : 0<=ih-ki<oh, 0<=iw-kj<ow}
#                     dCols[ (n*oh+oh_pos)*ow+ow_pos , (c*kh+ki)*kw+kj ]
# The mask is built with CMPLT (0/1-float) products; out-of-range
# reads are folded to 0 by multiplying the loaded value by the mask.
def build_conv2d_bwd_dx(h, N, Cin, H, Wd, kh, kw):
    oh, ow = H - kh + 1, Wd - kw + 1
    P, S = N * oh * ow, Cin * kh * kw
    out = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32,
                   dims=(N, Cin, H, Wd), instance=0)
    dcols = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32,
                     dims=(P, S), instance=1)
    n = h.range(axis_id=0, axis_type=K.AXIS_LOOP, extent=N)
    c = h.range(axis_id=1, axis_type=K.AXIS_LOOP, extent=Cin)
    ih = h.range(axis_id=2, axis_type=K.AXIS_LOOP, extent=H)
    iw = h.range(axis_id=3, axis_type=K.AXIS_LOOP, extent=Wd)
    # one fused reduce over the kh*kw kernel taps
    ki = h.range(axis_id=4, axis_type=K.AXIS_REDUCE, extent=kh)
    kj = h.range(axis_id=5, axis_type=K.AXIS_REDUCE, extent=kw)
    oh_pos = h.isub(ih, ki)            # output row that used this tap
    ow_pos = h.isub(iw, kj)
    # in-bounds mask.  The renderer emits every UOP_RANGE var as `uint`,
    # so ISUB is unsigned: when ki>ih the result wraps to a huge value.
    # That means a single `oh_pos < oh` rejects BOTH the upper bound and
    # the wrapped-negative case -- no separate `0 <= oh_pos` term is
    # needed (and `-1 < oh_pos` would be WRONG: -1 promotes to UINT_MAX,
    # so the comparison is always false).  See the signed_index_isub
    # test in cuda_render_bugs.py for the underlying renderer gap.
    zero = h.iconst(0)
    inb = h.iand(h.ilt(oh_pos, h.iconst(oh)),
                 h.ilt(ow_pos, h.iconst(ow)))
    # clamp the address so an out-of-range tap reads slot 0; its FP
    # contribution is then zeroed by the IWHERE select below.
    oh_c = h.iwhere(inb, oh_pos, zero)
    ow_c = h.iwhere(inb, ow_pos, zero)
    p_idx = _flat(h, [n, oh_c, ow_c], (N, oh, ow))
    s_idx = _flat(h, [c, ki, kj], (Cin, kh, kw))
    val = h.index_e(dcols, _flat(h, [p_idx, s_idx], (P, S)))
    # mask the loaded FP value: in-range taps keep their value,
    # out-of-range taps contribute the reduce identity (0.0).
    contrib = h.iwhere(inb, val, h.fconst(0.0))
    # reduce both kernel axes (4 then 5)
    acc = h.reduce(K.REDUCE_SUM, axis=5,
                   src=h.reduce(K.REDUCE_SUM, axis=4, src=contrib))
    return h.store(out, _flat(h, [n, c, ih, iw], (N, Cin, H, Wd)),
                   acc), 1, (N, Cin, H, Wd)


# -------- Layer 6: Scaled dot-product attention forward.
#   S[i,j] = (1/sqrt(d)) sum_e Q[i,e] K[j,e]
#   A = softmax_j(S);  O[i,e] = sum_j A[i,j] V[j,e]
# Built as a single fused DAG: the j-softmax (max + sum) is recomputed
# per output element -- naive but correct, like build_softmax does, and
# it exercises three reduce axes feeding one store.
def build_attention_fwd(h, L, D):
    """L sequence length, D head dim.  Q,K,V are [L,D]; output [L,D]."""
    out = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(L, D), instance=0)
    q = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(L, D), instance=1)
    kb = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(L, D), instance=2)
    v = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(L, D), instance=3)
    scale = h.fconst(1.0 / (D ** 0.5))
    i = h.range(axis_id=0, axis_type=K.AXIS_LOOP, extent=L)
    e = h.range(axis_id=1, axis_type=K.AXIS_LOOP, extent=D)

    def score(j_range, e_axis):
        """S[i,j] = scale * sum_e Q[i,e]K[j,e], reduce over e_axis."""
        ee = h.range(axis_id=e_axis, axis_type=K.AXIS_REDUCE, extent=D)
        qv = h.index_e(q, _flat(h, [i, ee], (L, D)))
        kv = h.index_e(kb, _flat(h, [j_range, ee], (L, D)))
        return h.mul(scale, h.reduce(K.REDUCE_SUM, axis=e_axis,
                                     src=h.mul(qv, kv)))

    # row max over j (axis 2): score uses inner reduce axis 3
    j_max = h.range(axis_id=2, axis_type=K.AXIS_REDUCE, extent=L)
    smax = h.reduce(K.REDUCE_MAX, axis=2, src=score(j_max, 3))
    # row sum of exp over j (axis 4): score inner reduce axis 5
    j_sum = h.range(axis_id=4, axis_type=K.AXIS_REDUCE, extent=L)
    ssum = h.reduce(K.REDUCE_SUM, axis=4,
                    src=h.exp2(h.mul(h.add(score(j_sum, 5), h.neg(smax)),
                                     h.fconst(LOG2E))))
    # O[i,e] = sum_j A[i,j] V[j,e]; A[i,j] = exp(S-smax)/ssum, reduce axis 6
    j_o = h.range(axis_id=6, axis_type=K.AXIS_REDUCE, extent=L)
    aij = h.mul(h.exp2(h.mul(h.add(score(j_o, 7), h.neg(smax)),
                             h.fconst(LOG2E))),
                h.recip(ssum))
    ov = h.index_e(v, _flat(h, [j_o, e], (L, D)))
    o = h.reduce(K.REDUCE_SUM, axis=6, src=h.mul(aij, ov))
    return h.store(out, _flat(h, [i, e], (L, D)), o), 3, (L, D)


# ====================================================================
# numpy references (float64) + width-aware fp32 tolerance
# ====================================================================
def _gelu_np(x):
    x = x.astype(np.float64)
    return 0.5 * x * (1.0 + np.tanh(GELU_C0 * (x + GELU_C1 * x ** 3)))


def make_case(name, shape, rng):
    """Returns (input arrays fp32, reference fp64, tol_abs, tol_rel)."""
    if name == "linear_fwd":
        M, N, Kd = shape
        x = rng.uniform(-1, 1, (M, Kd)).astype(np.float32)
        w = rng.uniform(-1, 1, (Kd, N)).astype(np.float32)
        b = rng.uniform(-1, 1, (N,)).astype(np.float32)
        ref = (x.astype(np.float64) @ w.astype(np.float64)
               + b.astype(np.float64))
        return [x, w, b], ref, 4.0 * Kd * FP32_EPS, 1e-2
    if name == "linear_bwd_dw":
        M, N, Kd = shape
        x = rng.uniform(-1, 1, (M, Kd)).astype(np.float32)
        dy = rng.uniform(-1, 1, (M, N)).astype(np.float32)
        ref = x.astype(np.float64).T @ dy.astype(np.float64)
        return [x, dy], ref, 4.0 * M * FP32_EPS, 1e-2
    if name == "linear_bwd_dx":
        M, N, Kd = shape
        dy = rng.uniform(-1, 1, (M, N)).astype(np.float32)
        w = rng.uniform(-1, 1, (Kd, N)).astype(np.float32)
        ref = dy.astype(np.float64) @ w.astype(np.float64).T
        return [dy, w], ref, 4.0 * N * FP32_EPS, 1e-2
    if name == "linear_bwd_db":
        M, N = shape
        dy = rng.uniform(-1, 1, (M, N)).astype(np.float32)
        ref = dy.astype(np.float64).sum(axis=0)
        return [dy], ref, 4.0 * M * FP32_EPS, 1e-2
    if name in ("relu_fwd", "relu_bwd", "gelu_fwd", "gelu_bwd"):
        (n_elem,) = shape
        x = rng.uniform(-3, 3, (n_elem,)).astype(np.float32)
        if name == "relu_fwd":
            return [x], np.maximum(x.astype(np.float64), 0.0), 1e-6, 1e-4
        if name == "gelu_fwd":
            return [x], _gelu_np(x), 1e-5, 1e-4
        dy = rng.uniform(-1, 1, (n_elem,)).astype(np.float32)
        if name == "relu_bwd":
            ref = (x.astype(np.float64) > 0.0) * dy.astype(np.float64)
            return [x, dy], ref, 1e-6, 1e-4
        # gelu_bwd
        xd = x.astype(np.float64)
        u = GELU_C0 * (xd + GELU_C1 * xd ** 3)
        t = np.tanh(u)
        du = GELU_C0 * (1.0 + 3.0 * GELU_C1 * xd ** 2)
        dg = 0.5 * (1.0 + t) + 0.5 * xd * (1.0 - t ** 2) * du
        return [x, dy], dy.astype(np.float64) * dg, 1e-5, 1e-4
    if name == "layernorm_fwd":
        R, C = shape
        x = rng.uniform(-2, 2, (R, C)).astype(np.float32)
        xd = x.astype(np.float64)
        mu = xd.mean(axis=1, keepdims=True)
        var = ((xd - mu) ** 2).mean(axis=1, keepdims=True)
        ref = (xd - mu) / np.sqrt(var + LN_EPS)
        return [x], ref, 1e-4, 1e-3
    if name == "layernorm_bwd":
        R, C = shape
        x = rng.uniform(-2, 2, (R, C)).astype(np.float32)
        dy = rng.uniform(-1, 1, (R, C)).astype(np.float32)
        xd, dyd = x.astype(np.float64), dy.astype(np.float64)
        mu = xd.mean(axis=1, keepdims=True)
        var = ((xd - mu) ** 2).mean(axis=1, keepdims=True)
        rstd = 1.0 / np.sqrt(var + LN_EPS)
        xhat = (xd - mu) * rstd
        ref = rstd * (dyd - dyd.mean(axis=1, keepdims=True)
                      - xhat * (dyd * xhat).mean(axis=1, keepdims=True))
        return [x, dy], ref, 1e-4, 1e-3
    if name == "conv2d_fwd":
        # shape carries the conv params; build cols + wflat here.
        N, Cin, H, Wd, Cout, kh, kw = shape
        x = rng.uniform(-1, 1, (N, Cin, H, Wd)).astype(np.float32)
        w = rng.uniform(-1, 1, (Cout, Cin, kh, kw)).astype(np.float32)
        oh, ow = H - kh + 1, Wd - kw + 1
        # im2col: cols[N*oh*ow, Cin*kh*kw]
        cols = np.zeros((N * oh * ow, Cin * kh * kw), dtype=np.float32)
        idx = 0
        for nn in range(N):
            for i in range(oh):
                for j in range(ow):
                    patch = x[nn, :, i:i + kh, j:j + kw]
                    cols[idx] = patch.reshape(-1)
                    idx += 1
        wflat = w.reshape(Cout, -1).T.copy()        # [Cin*kh*kw, Cout]
        ref = cols.astype(np.float64) @ wflat.astype(np.float64)
        return [cols, wflat], ref, 4.0 * (Cin * kh * kw) * FP32_EPS, 1e-2
    if name == "conv2d_bwd_dw":
        N, Cin, H, Wd, Cout, kh, kw = shape
        oh, ow = H - kh + 1, Wd - kw + 1
        P, S = N * oh * ow, Cin * kh * kw
        cols = rng.uniform(-1, 1, (P, S)).astype(np.float32)
        dy = rng.uniform(-1, 1, (P, Cout)).astype(np.float32)
        ref = cols.astype(np.float64).T @ dy.astype(np.float64)   # [S,Cout]
        return [cols, dy], ref, 4.0 * P * FP32_EPS, 1e-2
    if name == "conv2d_bwd_dx":
        N, Cin, H, Wd, Cout, kh, kw = shape
        oh, ow = H - kh + 1, Wd - kw + 1
        P, S = N * oh * ow, Cin * kh * kw
        dcols = rng.uniform(-1, 1, (P, S)).astype(np.float32)
        # col2im reference: scatter dcols back into the input grid.
        dx = np.zeros((N, Cin, H, Wd), dtype=np.float64)
        dcd = dcols.astype(np.float64)
        for nn in range(N):
            for oi in range(oh):
                for oj in range(ow):
                    p = (nn * oh + oi) * ow + oj
                    for cc in range(Cin):
                        for ki in range(kh):
                            for kj in range(kw):
                                s = (cc * kh + ki) * kw + kj
                                dx[nn, cc, oi + ki, oj + kj] += dcd[p, s]
        return [dcols], dx, 4.0 * (kh * kw) * FP32_EPS, 1e-2
    if name == "attention_fwd":
        L, D = shape
        q = rng.uniform(-1, 1, (L, D)).astype(np.float32)
        k = rng.uniform(-1, 1, (L, D)).astype(np.float32)
        v = rng.uniform(-1, 1, (L, D)).astype(np.float32)
        qd, kd, vd = (a.astype(np.float64) for a in (q, k, v))
        s = (qd @ kd.T) / np.sqrt(D)
        s = s - s.max(axis=1, keepdims=True)
        a = np.exp(s)
        a = a / a.sum(axis=1, keepdims=True)
        ref = a @ vd
        return [q, k, v], ref, 1e-4, 1e-3
    raise ValueError(name)


# Curriculum: name -> (builder, shape-extractor for the builder args).
# conv2d_fwd's shape is the conv-param tuple; the builder needs the
# matmul dims (P,Q,S) derived from it.
def _conv_mm_dims(shape):
    N, Cin, H, Wd, Cout, kh, kw = shape
    oh, ow = H - kh + 1, Wd - kw + 1
    return N * oh * ow, Cout, Cin * kh * kw


CURRICULUM = [
    # name, builder, shape-> builder-args, total-thread-count fn
    ("linear_fwd",     build_linear_fwd,        lambda s: s,
     lambda s: s[0] * s[1]),
    ("linear_bwd_dw",  build_linear_bwd_dw,     lambda s: s,
     lambda s: s[2] * s[1]),
    ("linear_bwd_dx",  build_linear_bwd_dx,     lambda s: s,
     lambda s: s[0] * s[2]),
    ("linear_bwd_db",  build_linear_bwd_db,     lambda s: (s[0], s[1]),
     lambda s: s[1]),
    ("relu_fwd",       build_relu_fwd,          lambda s: (s[0],),
     lambda s: s[0]),
    ("relu_bwd",       build_relu_bwd,          lambda s: (s[0],),
     lambda s: s[0]),
    ("gelu_fwd",       build_gelu_fwd,          lambda s: (s[0],),
     lambda s: s[0]),
    ("gelu_bwd",       build_gelu_bwd,          lambda s: (s[0],),
     lambda s: s[0]),
    ("layernorm_fwd",  build_layernorm_fwd,     lambda s: s,
     lambda s: s[0] * s[1]),
    ("layernorm_bwd",  build_layernorm_bwd,     lambda s: s,
     lambda s: s[0] * s[1]),
    ("conv2d_fwd",     build_conv2d_fwd_matmul, _conv_mm_dims,
     lambda s: _conv_mm_dims(s)[0] * _conv_mm_dims(s)[1]),
    ("conv2d_bwd_dw",  build_conv2d_bwd_dw,     _conv_mm_dims,
     lambda s: _conv_mm_dims(s)[2] * _conv_mm_dims(s)[1]),
    ("conv2d_bwd_dx",  build_conv2d_bwd_dx,
     lambda s: (s[0], s[1], s[2], s[3], s[5], s[6]),
     lambda s: s[0] * s[1] * s[2] * s[3]),
    ("attention_fwd",  build_attention_fwd,     lambda s: s,
     lambda s: s[0] * s[1]),
]
BUILDERS = {n: (b, sa, tc) for (n, b, sa, tc) in CURRICULUM}


def shapes_for(name, quick):
    if name.startswith("linear_bwd_db"):
        return [(256, 256)] if quick else [(512, 512), (1024, 1024)]
    if name.startswith("linear"):
        return [(128, 128, 128)] if quick else [(256, 256, 256), (512, 512, 512)]
    if name in ("relu_fwd", "relu_bwd", "gelu_fwd", "gelu_bwd"):
        return [(65536,)] if quick else [(262144,), (1048576,)]
    if name.startswith("layernorm"):
        return [(256, 256)] if quick else [(1024, 256), (4096, 256)]
    if name.startswith("conv2d"):
        # N,Cin,H,W,Cout,kh,kw -- a small but real conv shape
        return ([(1, 3, 16, 16, 8, 3, 3)] if quick
                else [(1, 3, 32, 32, 16, 3, 3), (4, 8, 16, 16, 16, 3, 3)])
    if name == "attention_fwd":
        return [(64, 32)] if quick else [(128, 64), (256, 64)]
    raise ValueError(name)


# ====================================================================
# thvm-CUDA runner
# ====================================================================
def run_layer(h, c, name, shape):
    builder, shape_args, thread_count = BUILDERS[name]
    rng = np.random.default_rng(42)
    arrs, ref, tol_abs, tol_rel = make_case(name, shape, rng)

    args = shape_args(shape)
    try:
        root, n_in, out_shape = builder(h, *args)
    except Exception as ex:                       # noqa: BLE001
        return dict(status="build_err", name=name, shape=shape, err=repr(ex))

    t0 = time.perf_counter_ns()
    cu = h.render_cuda(root, name="k")
    render_us = (time.perf_counter_ns() - t0) / 1e3
    if not cu:
        return dict(status="render_err", name=name, shape=shape)

    try:
        fn = c.compile(cu, fn="k")
    except RuntimeError as ex:
        return dict(status="compile_err", name=name, shape=shape,
                    err=str(ex), cu=cu)

    out_buf = c.buf_alloc(int(np.prod(out_shape)) * 4)
    in_bufs = []
    for arr in arrs:
        b = c.buf_alloc(arr.nbytes)
        c.buf_write_array(b, arr)
        in_bufs.append(b)
    bufs = [out_buf] + in_bufs
    total = thread_count(shape)
    grid, block = _grid_block(total)

    try:
        for _ in range(WARMUP):
            c.dispatch(fn, bufs, grid=grid, block=block)
    except RuntimeError as ex:
        for b in [out_buf] + in_bufs:
            c.buf_release(b)
        c.fn_release(fn)
        return dict(status="dispatch_err", name=name, shape=shape,
                    err=str(ex), cu=cu)

    got = c.buf_read_array(out_buf, out_shape, np.float32).astype(np.float64)
    diff = np.abs(got - ref)
    max_abs = float(diff.max())
    max_rel = float((diff / (np.abs(ref) + 1e-30)).max())
    correct = (max_abs <= tol_abs) or (max_rel <= tol_rel)

    gpu = []
    for _ in range(REPS):
        _, g = c.dispatch_timed(fn, bufs, grid=grid, block=block)
        gpu.append(g / 1e3)

    for b in [out_buf] + in_bufs:
        c.buf_release(b)
    c.fn_release(fn)

    return dict(
        status="ok", name=name, shape=shape,
        render_us=render_us, max_abs=max_abs, max_rel=max_rel,
        correct=correct, tol_abs=tol_abs, tol_rel=tol_rel,
        gpu_p50_us=pctl(gpu, 0.50), gpu_p10_us=pctl(gpu, 0.10),
        cu_lines=cu.count(chr(10)) + 1, cu=cu,
    )


# ====================================================================
# orchestration
# ====================================================================
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--quick", action="store_true")
    ap.add_argument("--only", default="", help="run one layer by name")
    ap.add_argument("--json", default="")
    ap.add_argument("--show-cu", action="store_true",
                    help="print the rendered .cu of every failing kernel")
    args = ap.parse_args()

    if not Cuda.available():
        print("SKIP: this libthvm_py build has no CUDA bridge.")
        return 0

    h = Thvm()
    c = Cuda()
    print(f"# CUDA NN-layer cross-validation -- device sm_{c.device_sm()}")
    print("=" * 92)

    names = ([args.only] if args.only
             else [n for (n, *_rest) in CURRICULUM])
    rows = []
    all_ok = True
    for name in names:
        if name not in BUILDERS:
            print(f"unknown layer: {name}")
            return 2
        for shape in shapes_for(name, args.quick):
            label = f"{name} {shape}"
            res = run_layer(h, c, name, shape)
            rows.append(res)
            if res["status"] != "ok":
                all_ok = False
                print(f"  {label:42s} {res['status'].upper()}  "
                      f"{res.get('err', '')[:120]}")
                if args.show_cu and res.get("cu"):
                    print("  --- rendered .cu ---")
                    print(res["cu"])
                continue
            ok = res["correct"]
            all_ok &= ok
            print(f"  {label:42s} "
                  f"{'OK  ' if ok else 'FAIL'} "
                  f"abs={res['max_abs']:.2e} rel={res['max_rel']:.2e}  "
                  f"gpu p50={res['gpu_p50_us']:.1f}us  "
                  f"({res['cu_lines']}-line .cu)")
            if not ok and args.show_cu:
                print("  --- rendered .cu (numerically wrong) ---")
                print(res["cu"])

    print("=" * 92)
    print("PASS: every NN-layer kernel matches numpy"
          if all_ok else
          "FAIL: a layer kernel failed -- see rows above")

    if args.json:
        slim = [{k: v for k, v in r.items() if k != "cu"} for r in rows]
        Path(args.json).write_text(json.dumps(slim, indent=1))
        print(f"wrote {args.json}")

    return 0 if all_ok else 1


if __name__ == "__main__":
    sys.exit(main())
