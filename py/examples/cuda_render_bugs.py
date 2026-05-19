"""CUDA renderer bug regression tests -- thvm-CUDA vs numpy.

Two renderer bugs surfaced by the NN-layer cross-validation arc
(docs/cuda_nn_xval.md).  Each builds a UOp DAG that exercises the bug,
renders it for CUDA, nvrtc-compiles, dispatches on the GPU, and checks
the result against a numpy reference.

  bug 1  nested_reduce_axis_dep
    out[i] = sum_j(in[i] * sum_k m[j,k]).  The inner REDUCE over k is
    wrapped in an elementwise MUL inside the outer REDUCE over j, and
    its body m[j,k] indexes by the OUTER reduce's axis var.  The inner
    reduce-loop must nest inside the outer reduce-loop; a flat hoist
    references the outer axis var before its declaration -- nvrtc
    `identifier "aN" is undefined`.  Blocks scaled-dot-product
    attention (softmax-over-scores nests a per-row reduce in the row
    reduce).

  bug 2  signed_index_isub
    out[i] = (i-1 >= 0) ? in[i-1] : 0, with the guard built as
    ILT(-1, i-1).  UOP_ISUB is a signed subtract, but RANGE vars are
    declared `uint`: `i-1` at i==0 wraps to ~UINT_MAX and the guard
    `-1 < (i-1)` promotes -1 to UINT_MAX, so it is always false and
    every live element is masked to 0.  The renderer must emit the
    integer expression signed.

Both bugs are FIXED -- this script asserts they stay fixed.  Runs only
on a Linux+CUDA host (V100 / SM70 pod); without the CUDA bridge it
prints a SKIP line and exits 0.

  python3 py/examples/cuda_render_bugs.py
"""
from __future__ import annotations

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


# ====================================================================
# bug 1 -- nested_reduce_axis_dep
# ====================================================================
def build_nested_reduce(h, I, J, Kd):
    """out[i] = sum_j(in[i] * sum_k m[j,k]).

    REDUCE(MUL(in[i], REDUCE(m[j*Kd+k], SUM, k)), SUM, j).  The inner
    reduce's body indexes m by the outer reduce's axis var `j`.
    """
    out_b = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(I,),
                     instance=0)
    in_b = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(I,),
                    instance=1)
    m_b = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(J, Kd),
                   instance=2)
    i = h.range(axis_id=0, axis_type=K.AXIS_LOOP, extent=I)
    j = h.range(axis_id=1, axis_type=K.AXIS_REDUCE, extent=J)
    k = h.range(axis_id=2, axis_type=K.AXIS_REDUCE, extent=Kd)
    # m[j,k] = m[j*Kd + k]
    jk = h.iadd(h.imul(j, h.iconst(Kd)), k)
    inner = h.reduce(K.REDUCE_SUM, axis=2, src=h.index_e(m_b, jk))
    body = h.mul(h.index_e(in_b, i), inner)
    outer = h.reduce(K.REDUCE_SUM, axis=1, src=body)
    return h.store(out_b, i, outer)


def run_bug1(h, c):
    I, J, Kd = 7, 5, 11
    rng = np.random.default_rng(1)
    in_a = rng.standard_normal(I).astype(np.float32)
    m_a = rng.standard_normal((J, Kd)).astype(np.float32)
    # ref: out[i] = in[i] * sum over all of m
    ref = (in_a.astype(np.float64)
           * m_a.astype(np.float64).sum())

    root = build_nested_reduce(h, I, J, Kd)
    cu = h.render_cuda(root, name="k")
    if not cu:
        return dict(name="nested_reduce_axis_dep", status="render_err")
    try:
        fn = c.compile(cu, fn="k")
    except RuntimeError as e:
        return dict(name="nested_reduce_axis_dep", status="compile_err",
                    detail=str(e), cu=cu)

    out_buf = c.buf_alloc(I * 4)
    in_buf = c.buf_alloc(in_a.nbytes)
    m_buf = c.buf_alloc(m_a.nbytes)
    c.buf_write_array(in_buf, in_a)
    c.buf_write_array(m_buf, m_a)
    grid, block = _grid_block(I)
    c.dispatch(fn, [out_buf, in_buf, m_buf], grid=grid, block=block)
    got = c.buf_read_array(out_buf, (I,), np.float32).astype(np.float64)
    c.buf_release(out_buf)
    c.buf_release(in_buf)
    c.buf_release(m_buf)
    c.fn_release(fn)

    max_abs = float(np.abs(got - ref).max())
    tol = 1e-4 * max(1.0, float(np.abs(ref).max()))
    return dict(name="nested_reduce_axis_dep", status="ok",
                correct=(max_abs <= tol), max_abs=max_abs, tol=tol)


# ====================================================================
# bug 2 -- signed_index_isub
# ====================================================================
def build_signed_isub(h, N):
    """out[i] = (i-1 >= 0) ? in[i-1] : 0.

    Guard built as ILT(-1, i-1): a signed comparison of -1 against a
    UOP_ISUB result.  In unsigned space this is always false.
    """
    out_b = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(N,),
                     instance=0)
    in_b = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(N,),
                    instance=1)
    i = h.range(axis_id=0, axis_type=K.AXIS_LOOP, extent=N)
    im1 = h.isub(i, h.iconst(1))                 # i - 1  (negative at i=0)
    neg1 = h.iconst(-1)
    guard = h.ilt(neg1, im1)                     # -1 < (i-1)  <=>  i-1 >= 0
    ld = h.index_e(in_b, im1)
    sel = h.iwhere(guard, ld, h.fconst(0.0))
    return h.store(out_b, i, sel)


def run_bug2(h, c):
    N = 64
    rng = np.random.default_rng(2)
    in_a = (rng.standard_normal(N).astype(np.float32) + 10.0)
    ref = np.zeros(N, dtype=np.float64)
    ref[1:] = in_a[:-1].astype(np.float64)       # out[i] = in[i-1], out[0]=0

    root = build_signed_isub(h, N)
    cu = h.render_cuda(root, name="k")
    if not cu:
        return dict(name="signed_index_isub", status="render_err")
    try:
        fn = c.compile(cu, fn="k")
    except RuntimeError as e:
        return dict(name="signed_index_isub", status="compile_err",
                    detail=str(e), cu=cu)

    out_buf = c.buf_alloc(N * 4)
    in_buf = c.buf_alloc(in_a.nbytes)
    c.buf_write_array(in_buf, in_a)
    grid, block = _grid_block(N)
    c.dispatch(fn, [out_buf, in_buf], grid=grid, block=block)
    got = c.buf_read_array(out_buf, (N,), np.float32).astype(np.float64)
    c.buf_release(out_buf)
    c.buf_release(in_buf)
    c.fn_release(fn)

    max_abs = float(np.abs(got - ref).max())
    return dict(name="signed_index_isub", status="ok",
                correct=(max_abs <= 1e-5), max_abs=max_abs, tol=1e-5)


# ====================================================================
# driver
# ====================================================================
def main():
    if not Cuda.available():
        print("SKIP: this libthvm_py build has no CUDA bridge "
              "(run on the Linux+CUDA pod)")
        return 0

    h = Thvm()
    c = Cuda()
    sm = c.device_sm()
    print(f"CUDA device: SM{sm}")
    print()

    results = [run_bug1(h, c), run_bug2(h, c)]

    n_fail = 0
    for r in results:
        if r["status"] != "ok":
            n_fail += 1
            print(f"  FAIL  {r['name']}: {r['status']}")
            if "detail" in r:
                print(f"        {r['detail'].strip()}")
            continue
        if r["correct"]:
            print(f"  ok    {r['name']}: "
                  f"max_abs {r['max_abs']:.2e} <= {r['tol']:.0e}")
        else:
            n_fail += 1
            print(f"  FAIL  {r['name']}: "
                  f"max_abs {r['max_abs']:.2e} > {r['tol']:.0e}")

    print()
    if n_fail:
        print(f"{n_fail}/{len(results)} renderer-bug tests FAILED")
        return 1
    print(f"all {len(results)} renderer-bug tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
