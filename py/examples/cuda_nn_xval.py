"""NN-layer cross-validation: thvm-CUDA vs numpy.

Cross-validates whole neural-net layers -- not just single ops -- on the
CUDA target.  Each layer is built as a UOp DAG via py.thvm, rendered for
CUDA, nvrtc-compiled, dispatched on the GPU, and checked against a numpy
fp64 reference.  This is the layer-level companion to cuda_xval.py (which
measures single-op speed) and surfaced the two renderer bugs tracked in
docs/cuda_nn_xval.md / py/examples/cuda_render_bugs.py.

The headline layer is scaled-dot-product attention.  Its softmax-over-
scores nests a per-row reduce (the row max / row sum-of-exp) inside the
per-key reduce that forms the output -- exactly the nested-reduce shape
of renderer bug 1.  Attention forward AND backward are cross-validated.

  python3 py/examples/cuda_nn_xval.py                  # all layers
  python3 py/examples/cuda_nn_xval.py --only attention  # one layer

Runs only on a Linux+CUDA host (the pod is a V100 / SM70).  Without the
CUDA bridge it prints a SKIP line and exits 0.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).parent.parent.parent
sys.path.insert(0, str(ROOT))

from py.thvm import Thvm, Cuda, K


def _grid_block(total):
    """1-D launch: total threads cover the output; block is warp-multiple."""
    block = 256 if total >= 256 else max(32, ((total + 31) // 32) * 32)
    grid = (total + block - 1) // block
    return grid, block


def _run_kernel(c, cu, out_numel, in_arrays):
    """nvrtc-compile `cu`, dispatch with out + in_arrays, return out fp64.

    Buffer order matches the renderer: out (instance 0) first, then the
    inputs in instance order.
    """
    fn = c.compile(cu, fn="k")
    out_buf = c.buf_alloc(out_numel * 4)
    in_bufs = []
    for arr in in_arrays:
        # The renderer's kernels are fp32 throughout; a float64 input
        # (a numpy promotion footgun -- e.g. `arr / python_float`)
        # would write 8-byte elements the kernel reads as 4-byte ones.
        a32 = np.ascontiguousarray(arr, dtype=np.float32)
        b = c.buf_alloc(a32.nbytes)
        c.buf_write_array(b, a32)
        in_bufs.append(b)
    grid, block = _grid_block(out_numel)
    c.dispatch(fn, [out_buf] + in_bufs, grid=grid, block=block)
    got = c.buf_read_array(out_buf, (out_numel,), np.float32).astype(np.float64)
    c.buf_release(out_buf)
    for b in in_bufs:
        c.buf_release(b)
    c.fn_release(fn)
    return got


# ====================================================================
# attention -- the nested-reduce layer
# ====================================================================
def build_attention_fwd(h, Tq, Tk, D):
    """O[i,d] = sum_j( exp(S[i,j] - m_i) * V[j,d] ) / Z_i.

    A single fused kernel: m_i = max_k S[i,k] and Z_i = sum_k
    exp(S[i,k] - m_i) are REDUCEs over the key axis k, computed INSIDE
    the per-key reduce over j that forms the output.  Their bodies index
    S by `i` (an output axis) -- and the kernel as a whole nests the
    k-reduce inside the j-reduce.  This is renderer bug 1's shape: an
    inner reduce wrapped in an elementwise op, emitted at the loop depth
    its axis dependencies require.

    Inputs: S (Tq,Tk) pre-scaled scores, V (Tk,D).  Output: O (Tq,D).
    """
    o_buf = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(Tq, D),
                     instance=0)
    s_buf = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(Tq, Tk),
                     instance=1)
    v_buf = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(Tk, D),
                     instance=2)
    i = h.range(axis_id=0, axis_type=K.AXIS_LOOP, extent=Tq)   # query row
    d = h.range(axis_id=1, axis_type=K.AXIS_LOOP, extent=D)    # head dim
    j = h.range(axis_id=2, axis_type=K.AXIS_REDUCE, extent=Tk)  # output key
    km = h.range(axis_id=3, axis_type=K.AXIS_REDUCE, extent=Tk)  # max key
    kz = h.range(axis_id=4, axis_type=K.AXIS_REDUCE, extent=Tk)  # sum key
    Tk_c = h.iconst(Tk)
    D_c = h.iconst(D)

    # m_i = max_k S[i,k]
    m_addr = h.iadd(h.imul(i, Tk_c), km)
    m_i = h.reduce(K.REDUCE_MAX, axis=3, src=h.index_e(s_buf, m_addr))
    # Z_i = sum_k exp(S[i,k] - m_i)
    z_addr = h.iadd(h.imul(i, Tk_c), kz)
    z_e = h.exp(h.add(h.index_e(s_buf, z_addr), h.neg(m_i)))
    Z_i = h.reduce(K.REDUCE_SUM, axis=4, src=z_e)
    # O[i,d] = sum_j exp(S[i,j] - m_i) * V[j,d]   (then / Z_i)
    s_ij = h.index_e(s_buf, h.iadd(h.imul(i, Tk_c), j))
    p_ij = h.exp(h.add(s_ij, h.neg(m_i)))
    v_jd = h.index_e(v_buf, h.iadd(h.imul(j, D_c), d))
    acc = h.reduce(K.REDUCE_SUM, axis=2, src=h.mul(p_ij, v_jd))
    o_val = h.mul(acc, h.recip(Z_i))
    o_addr = h.iadd(h.imul(i, D_c), d)
    return h.store(o_buf, o_addr, o_val), Tq * D, [s_buf, v_buf]


def build_attention_bwd_dscore(h, Tq, Tk):
    """dS[i,j] = P[i,j] * (G[i,j] - sum_k P[i,k] * G[i,k]).

    The softmax-backward Jacobian-vector product.  The correction term
    rowdot_i = sum_k P[i,k]*G[i,k] is a REDUCE over the key axis k that
    must be computed once per row and reused across every j -- and its
    body indexes P and G by `i`.  Building dS[i,j] in one kernel nests
    the k-reduce inside the per-j store: bug 1's shape on the backward
    pass.  P = the attention probabilities, G = the upstream gradient
    dP = dO @ V^T.

    Inputs: P (Tq,Tk), G (Tq,Tk).  Output: dS (Tq,Tk).
    """
    ds_buf = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(Tq, Tk),
                      instance=0)
    p_buf = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(Tq, Tk),
                     instance=1)
    g_buf = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(Tq, Tk),
                     instance=2)
    i = h.range(axis_id=0, axis_type=K.AXIS_LOOP, extent=Tq)   # query row
    j = h.range(axis_id=1, axis_type=K.AXIS_LOOP, extent=Tk)   # output key
    k = h.range(axis_id=2, axis_type=K.AXIS_REDUCE, extent=Tk)  # correction key
    Tk_c = h.iconst(Tk)

    # rowdot_i = sum_k P[i,k] * G[i,k]
    k_addr = h.iadd(h.imul(i, Tk_c), k)
    rowdot = h.reduce(K.REDUCE_SUM, axis=2,
                      src=h.mul(h.index_e(p_buf, k_addr),
                                h.index_e(g_buf, k_addr)))
    # dS[i,j] = P[i,j] * (G[i,j] - rowdot_i)
    ij = h.iadd(h.imul(i, Tk_c), j)
    p_ij = h.index_e(p_buf, ij)
    g_ij = h.index_e(g_buf, ij)
    ds_val = h.mul(p_ij, h.add(g_ij, h.neg(rowdot)))
    return h.store(ds_buf, ij, ds_val), Tq * Tk, [p_buf, g_buf]


def ref_attention(S, V):
    """numpy fp64 reference: O = softmax(S, axis=-1) @ V.  Returns O, P."""
    S = S.astype(np.float64)
    V = V.astype(np.float64)
    m = S.max(axis=-1, keepdims=True)
    e = np.exp(S - m)
    P = e / e.sum(axis=-1, keepdims=True)
    return P @ V, P


def ref_attention_bwd_dscore(P, G):
    """numpy fp64 reference for softmax-backward dS."""
    P = P.astype(np.float64)
    G = G.astype(np.float64)
    rowdot = (P * G).sum(axis=-1, keepdims=True)
    return P * (G - rowdot)


def run_attention(h, c):
    Tq, Tk, D = 24, 40, 16
    rng = np.random.default_rng(7)
    # `/ sqrt(D)` divides by a Python float, which would promote the
    # array back to float64; cast AFTER scaling so the kernel input is
    # genuinely fp32 (the renderer reads 4-byte elements).
    S = (rng.standard_normal((Tq, Tk)) / np.sqrt(D)).astype(np.float32)
    V = rng.standard_normal((Tk, D)).astype(np.float32)

    results = []

    # --- forward ---------------------------------------------------
    O_ref, P_ref = ref_attention(S, V)
    root, numel, ins = build_attention_fwd(h, Tq, Tk, D)
    cu = h.render_cuda(root, name="k")
    if not cu:
        results.append(("attention/forward", "render_err", None))
    else:
        try:
            got = _run_kernel(c, cu, numel,
                              [S.reshape(-1), V.reshape(-1)])
            O_got = got.reshape(Tq, D)
            max_abs = float(np.abs(O_got - O_ref).max())
            results.append(("attention/forward", "ok",
                            (max_abs, 1e-4)))
        except RuntimeError as e:
            results.append(("attention/forward", f"err: {e}", None))

    # --- backward (softmax-backward dS) ----------------------------
    # Upstream gradient G = dP; use a random gradient on the output O,
    # propagated to P-space as dP = dO @ V^T (the chain through P@V).
    dO = rng.standard_normal((Tq, D)).astype(np.float32)
    G = (dO.astype(np.float64) @ V.astype(np.float64).T)
    dS_ref = ref_attention_bwd_dscore(P_ref, G)
    root_b, numel_b, ins_b = build_attention_bwd_dscore(h, Tq, Tk)
    cu_b = h.render_cuda(root_b, name="k")
    if not cu_b:
        results.append(("attention/backward", "render_err", None))
    else:
        try:
            got_b = _run_kernel(c, cu_b, numel_b,
                                [P_ref.astype(np.float32).reshape(-1),
                                 G.astype(np.float32).reshape(-1)])
            dS_got = got_b.reshape(Tq, Tk)
            max_abs = float(np.abs(dS_got - dS_ref).max())
            results.append(("attention/backward", "ok",
                            (max_abs, 1e-4)))
        except RuntimeError as e:
            results.append(("attention/backward", f"err: {e}", None))

    return results


# ====================================================================
# softmax -- a single-reduce-pair baseline layer
# ====================================================================
def build_softmax(h, R, C):
    """out[r,c] = exp(x[r,c] - m_r) / Z_r.  Two row reduces feed an
    elementwise column output -- the simpler reduce-feeding-broadcast
    shape (no reduce-inside-reduce).
    """
    o_buf = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(R, C),
                     instance=0)
    x_buf = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(R, C),
                     instance=1)
    r = h.range(axis_id=0, axis_type=K.AXIS_LOOP, extent=R)
    cc = h.range(axis_id=1, axis_type=K.AXIS_LOOP, extent=C)
    km = h.range(axis_id=2, axis_type=K.AXIS_REDUCE, extent=C)
    kz = h.range(axis_id=3, axis_type=K.AXIS_REDUCE, extent=C)
    C_c = h.iconst(C)
    m_r = h.reduce(K.REDUCE_MAX, axis=2,
                   src=h.index_e(x_buf, h.iadd(h.imul(r, C_c), km)))
    z_e = h.exp(h.add(h.index_e(x_buf, h.iadd(h.imul(r, C_c), kz)),
                      h.neg(m_r)))
    Z_r = h.reduce(K.REDUCE_SUM, axis=3, src=z_e)
    out_e = h.exp(h.add(h.index_e(x_buf, h.iadd(h.imul(r, C_c), cc)),
                        h.neg(m_r)))
    return (h.store(o_buf, h.iadd(h.imul(r, C_c), cc),
                    h.mul(out_e, h.recip(Z_r))),
            R * C, [x_buf])


def run_softmax(h, c):
    R, C = 32, 48
    rng = np.random.default_rng(11)
    x = rng.standard_normal((R, C)).astype(np.float32)
    xd = x.astype(np.float64)
    e = np.exp(xd - xd.max(axis=-1, keepdims=True))
    ref = e / e.sum(axis=-1, keepdims=True)
    root, numel, ins = build_softmax(h, R, C)
    cu = h.render_cuda(root, name="k")
    if not cu:
        return [("softmax/forward", "render_err", None)]
    try:
        got = _run_kernel(c, cu, numel, [x.reshape(-1)]).reshape(R, C)
        return [("softmax/forward", "ok",
                 (float(np.abs(got - ref).max()), 1e-4))]
    except RuntimeError as e:
        return [("softmax/forward", f"err: {e}", None)]


# ====================================================================
# driver
# ====================================================================
LAYERS = {
    "attention": run_attention,
    "softmax": run_softmax,
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only", choices=sorted(LAYERS),
                    help="run a single layer")
    args = ap.parse_args()

    if not Cuda.available():
        print("SKIP: this libthvm_py build has no CUDA bridge "
              "(run on the Linux+CUDA pod)")
        return 0

    h = Thvm()
    c = Cuda()
    print(f"CUDA device: SM{c.device_sm()}")
    print()

    names = [args.only] if args.only else sorted(LAYERS)
    n_fail = 0
    n_total = 0
    for name in names:
        for tag, status, band in LAYERS[name](h, c):
            n_total += 1
            if status == "ok":
                max_abs, tol = band
                if max_abs <= tol:
                    print(f"  ok    {tag}: max_abs {max_abs:.2e} <= {tol:.0e}")
                else:
                    n_fail += 1
                    print(f"  FAIL  {tag}: max_abs {max_abs:.2e} > {tol:.0e}")
            else:
                n_fail += 1
                print(f"  FAIL  {tag}: {status}")

    print()
    if n_fail:
        print(f"{n_fail}/{n_total} cross-validations FAILED")
        return 1
    print(f"all {n_total} cross-validations passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
