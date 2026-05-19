"""CUDA renderer bugs surfaced by the NN-layer cross-validation.

Per feedback_never_hide_bugs: when a kernel surfaces a renderer or
lowering bug, it gets a NORMAL failing test here -- not a skip, not a
work-around buried in the harness.  Each test below builds the minimal
UOp DAG that exposes the bug, renders it to CUDA, and asserts the
result is correct.  A test that FAILS is the point: it documents a
real gap for whoever fixes the renderer.

Run on the V100 pod:  python3 py/examples/cuda_render_bugs.py
Exits non-zero while any documented bug is still open.

  --- open bugs ---
  signed_index_isub : UOP_ISUB is documented "signed integer subtract"
    (src/thvm.h:368) but the CUDA renderer emits every UOP_RANGE var,
    and therefore every ISUB, as `uint`.  An ISUB that goes negative
    wraps to a huge unsigned value; a comparison against a negative
    literal (`-1 < x`) is then evaluated with `-1` promoted to
    UINT_MAX and is always false.  This silently breaks any DAG that
    relies on signed index arithmetic -- e.g. a col2im lower-bound
    check `0 <= ih - ki`.  The conv2d_bwd_dx harness layer works
    around it by relying on unsigned wraparound (`x < hi` rejects both
    the upper bound and the wrapped negative), but a DAG that needs a
    genuine signed comparison has no correct lowering today.

  nested_reduce_axis_dep : a REDUCE whose body contains ANOTHER REDUCE
    over a different axis -- where the inner reduce's body indexes by
    the OUTER reduce's axis variable -- is not kept nested.  The
    chain-reduce path in render_uop.c (rmu_emit_store_reduce) handles
    only DIRECTLY nested reduces (REDUCE(REDUCE(...)) with no op
    between).  When the inner reduce is wrapped in any elementwise op
    (e.g. MUL), it falls to the generic post-order hoist
    (rmu_collect_reduces), which emits every reduce loop at the top
    level.  The inner loop then (a) references the outer reduce's axis
    var before it is declared -> `undeclared identifier`, and (b) is
    computed ONCE instead of once per outer-axis value -> wrong result
    even if it compiled.  This blocks scaled-dot-product attention:
    softmax over the j-scores inherently nests a per-j score reduce
    (over the head dim) inside the max/sum reduce over j.  Needs a
    renderer design change -- port tinygrad's reduce nesting (each
    reduce emitted at the loop depth its body's axis deps require).
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).parent.parent.parent
sys.path.insert(0, str(ROOT))

from py.thvm import Thvm, Cuda, K


def _dispatch(c, cu, out_shape, in_arrs, total):
    fn = c.compile(cu, fn="k")
    out_buf = c.buf_alloc(int(np.prod(out_shape)) * 4)
    in_bufs = []
    for a in in_arrs:
        b = c.buf_alloc(a.nbytes)
        c.buf_write_array(b, a)
        in_bufs.append(b)
    block = max(32, ((total + 31) // 32) * 32)
    c.dispatch(fn, [out_buf] + in_bufs, grid=1, block=block)
    got = c.buf_read_array(out_buf, out_shape, np.float32)
    for b in [out_buf] + in_bufs:
        c.buf_release(b)
    c.fn_release(fn)
    return got


# ====================================================================
# BUG: signed ISUB renders as unsigned -> negative-literal compare
#      always false.
#
# Minimal DAG:  out[i] = (ISUB(i, k) compared to bounds) ? in[clamp] : 0
# reduced over k.  With i in 0..N and k in 0..N, half the (i,k) pairs
# have i-k < 0.  A correct signed `-1 < (i-k)` lower-bound check should
# pass for i-k >= 0 and fail for i-k < 0.  Under the uint renderer it
# fails for ALL pairs, so the reduce sums nothing and out is all-zero.
# ====================================================================
def test_signed_index_isub(h, c):
    """out[i] = sum_k [ -1 < (i-k) < N ] * in[i-k clamped].

    Reference: out[i] = sum_{k : 0 <= i-k} in[i-k] = sum_{j=0..i} in[j].
    A correct signed renderer gives the prefix sums of `in`.  The
    current uint renderer makes the `-1 < (i-k)` term always false,
    so every out[i] comes back 0.0.
    """
    N = 8
    out = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(N,), instance=0)
    inb = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(N,), instance=1)
    i = h.range(axis_id=0, axis_type=K.AXIS_LOOP, extent=N)
    k = h.range(axis_id=1, axis_type=K.AXIS_REDUCE, extent=N)
    diff = h.isub(i, k)
    # signed lower bound: -1 < diff   (i.e. diff >= 0)
    lo = h.ilt(h.isub(h.iconst(0), h.iconst(1)), diff)
    hi = h.ilt(diff, h.iconst(N))
    inb_mask = h.iand(lo, hi)
    addr = h.iwhere(inb_mask, diff, h.iconst(0))
    val = h.iwhere(inb_mask, h.index_e(inb, addr), h.fconst(0.0))
    root = h.store(out, i, h.reduce(K.REDUCE_SUM, axis=1, src=val))

    cu = h.render_cuda(root, name="k")
    assert cu, "render returned empty"
    x = np.arange(1, N + 1, dtype=np.float32)        # in[j] = j+1
    got = _dispatch(c, cu, (N,), [x], total=N).astype(np.float64)
    ref = np.cumsum(x.astype(np.float64))            # prefix sums

    max_abs = float(np.abs(got - ref).max())
    ok = max_abs <= 1e-4
    print(f"  signed_index_isub : got={got.tolist()}")
    print(f"                      ref={ref.tolist()}")
    print(f"                      max_abs={max_abs:.3e}  "
          f"{'OK' if ok else 'FAIL -- signed ISUB renders as uint'}")
    return ok


# ====================================================================
# BUG: a REDUCE nested (through an elementwise op) inside another
#      REDUCE, where the inner reduce's body depends on the outer
#      reduce's axis var, is emitted at the wrong loop nesting level.
#
# Minimal DAG:  out[i] = sum_j ( in[i] * sum_k m[j,k] )
# The inner reduce (sum_k m[j,k]) indexes m by `j`, the OUTER reduce's
# axis.  A correct renderer emits the k-loop INSIDE the j-loop, so the
# inner accumulator is recomputed for each j.  The current renderer
# hoists the k-loop above the j-loop -- its body then references the
# not-yet-declared `aj`, and even were it to compile it would compute
# sum_k m[j,k] exactly once with a stale j.
# ====================================================================
def test_nested_reduce_axis_dep(h, c):
    """out[i] = sum_j ( in[i] * sum_k m[j,k] ).

    Reference: out[i] = in[i] * sum_{j,k} m[j,k]  (the inner sum does
    NOT depend on i, so it factors -- but it DOES depend on j and must
    be recomputed per j).  A correct renderer matches numpy; the
    current one fails to compile (undeclared inner-loop axis var) or,
    if it compiled, would use a stale j.
    """
    N, M = 4, 3
    out = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(N,), instance=0)
    inb = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(N,), instance=1)
    mb = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(N, M), instance=2)
    i = h.range(axis_id=0, axis_type=K.AXIS_LOOP, extent=N)
    j = h.range(axis_id=1, axis_type=K.AXIS_REDUCE, extent=N)
    k = h.range(axis_id=2, axis_type=K.AXIS_REDUCE, extent=M)
    inner = h.reduce(K.REDUCE_SUM, axis=2,
                     src=h.index_e(mb, h.iadd(h.imul(j, h.iconst(M)), k)))
    body = h.mul(h.index_e(inb, i), inner)
    root = h.store(out, i, h.reduce(K.REDUCE_SUM, axis=1, src=body))

    cu = h.render_cuda(root, name="k")
    assert cu, "render returned empty"
    x = np.arange(1, N + 1, dtype=np.float32)
    m = np.arange(1, N * M + 1, dtype=np.float32).reshape(N, M)
    ref = x.astype(np.float64) * m.astype(np.float64).sum()

    try:
        got = _dispatch(c, cu, (N,), [x, m], total=N).astype(np.float64)
    except RuntimeError as ex:
        print(f"  nested_reduce_axis_dep : COMPILE/DISPATCH FAILED")
        print(f"                           {str(ex)[:160]}")
        print(f"                           FAIL -- inner reduce hoisted "
              f"above outer reduce loop")
        return False

    max_abs = float(np.abs(got - ref).max())
    ok = max_abs <= 1e-3
    print(f"  nested_reduce_axis_dep : got={got.tolist()}")
    print(f"                           ref={ref.tolist()}")
    print(f"                           max_abs={max_abs:.3e}  "
          f"{'OK' if ok else 'FAIL -- inner reduce uses stale outer axis'}")
    return ok


def main():
    if not Cuda.available():
        print("SKIP: no CUDA bridge in this build.")
        return 0
    h = Thvm()
    c = Cuda()
    print(f"# CUDA renderer-bug tests -- device sm_{c.device_sm()}")
    print("=" * 70)
    results = {
        "signed_index_isub": test_signed_index_isub(h, c),
        "nested_reduce_axis_dep": test_nested_reduce_axis_dep(h, c),
    }
    print("=" * 70)
    n_fail = sum(1 for v in results.values() if not v)
    if n_fail:
        print(f"FAIL: {n_fail} documented renderer bug(s) still open.")
        return 1
    print("PASS: all previously-documented renderer bugs are fixed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
