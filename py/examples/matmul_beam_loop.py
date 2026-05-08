"""End-to-end BEAM autotune loop driven from Python via thvm:

  1. Build matmul UOp DAG.
  2. propose() -> KOpt candidates from thvm's autotune proposer.
  3. For each candidate: apply_opt() -> mutates DAG, render() -> MSL.
  4. Compile + dispatch in-process via Metal helpers.
  5. Compare to numpy reference; pick winner by latency.

This is the smallest demonstrably-real consumer of Phase E -- propose,
apply_opt, and the renderer all reading/writing the UOp DAG, with no
KpSchedule / tile_uops side-channels involved.
"""
from __future__ import annotations

import sys
import time
from pathlib import Path

import numpy as np

ROOT = Path(__file__).parent.parent.parent
sys.path.insert(0, str(ROOT))

from py.thvm import Thvm, Metal, K
from py.examples.matmul_demo import build_matmul


def time_kernel(m: Metal, msl: str, M: int, N: int, K_dim: int,
                grid, threadgroup, reps: int = 20, warmup: int = 3):
    pso = m.compile_msl(msl, fn="k")
    rng = np.random.default_rng(42)
    a_np = rng.uniform(-1, 1, (M, K_dim)).astype(np.float32)
    b_np = rng.uniform(-1, 1, (K_dim, N)).astype(np.float32)
    c_ref = a_np @ b_np

    out_buf = m.buf_alloc(M * N * 4)
    a_buf = m.buf_alloc(M * K_dim * 4)
    b_buf = m.buf_alloc(K_dim * N * 4)
    m.buf_write_array(a_buf, a_np)
    m.buf_write_array(b_buf, b_np)

    for _ in range(warmup):
        m.dispatch(pso, [out_buf, a_buf, b_buf],
                   grid=grid, threadgroup=threadgroup)
    c_got = m.buf_read_array(out_buf, (M, N), np.float32)
    max_abs = float(np.max(np.abs(c_got - c_ref)))
    correct = max_abs <= 1e-2

    samples = []
    for _ in range(reps):
        wall, _gpu = m.dispatch_timed(pso, [out_buf, a_buf, b_buf],
                                      grid=grid, threadgroup=threadgroup)
        samples.append(wall)
    samples.sort()

    m.buf_release(out_buf); m.buf_release(a_buf); m.buf_release(b_buf)
    m.pso_release(pso)

    return dict(p10_ns=samples[max(0, reps // 10 - 1)],
                p50_ns=samples[reps // 2],
                gflops_p50=(2.0 * M * N * K_dim) / (samples[reps // 2] * 1e-9) / 1e9,
                correct=correct, max_abs=max_abs)


def make_kernel(M, N, K_dim, *, kopts):
    """Build matmul UOp + apply each KOpt in order; return (root, msl)."""
    h = Thvm()
    root = build_matmul(h, M, N, K_dim, with_tc=False)
    kid = h.kernel_alloc()
    out_buf = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(M, N), instance=0)
    a_buf = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(M, K_dim), instance=1)
    b_buf = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(K_dim, N), instance=2)
    h.kernel_set_cached_lift(kid, root, out_buf, [a_buf, b_buf])

    for kopt in kopts:
        new_root = h.kernel_apply_opt(kid, kopt)
        if new_root is None:
            raise RuntimeError(f"kernel_apply_opt({kopt}) failed")
        root = new_root
    return kid, root, h.render(root, name="k")


def dispatch_for(M, N, K_dim, *, parallel: bool):
    if parallel:
        n_tiles = (M // 8) * (N // 8)
        return (n_tiles * 32, 1, 1), (32, 1, 1)
    return (32, 1, 1), (32, 1, 1)


def main() -> int:
    h = Thvm()
    m = Metal()

    M, N, K_dim = 64, 64, 64

    # 1. propose() -- get the candidate set thvm's autotuner suggests.
    _kid, _root, _ = make_kernel(M, N, K_dim, kopts=[])
    cands = h.kernel_opts_propose(_kid, cap=64)
    print(f"propose: {len(cands)} candidates: {cands}")
    print()

    # 2. For each TC candidate, also try variants that pair TC with
    #    GLOBAL on m,n (parallel TC).  That's the full search this
    #    slice unblocks.
    KOP_NAMES = {0: "NONE", 9: "TC", 10: "GLOBAL"}
    print(f"{'config':>40s}  {'p50_us':>10}  {'p10_us':>10}  {'gflops':>8}  correct")
    print("-" * 90)

    rows = []
    for cand in cands:
        # Variant A: just TC (sequential, sgi==0 guard).
        kopts = [cand]
        kid, root, msl = make_kernel(M, N, K_dim, kopts=kopts)
        grid, tg = dispatch_for(M, N, K_dim, parallel=False)
        s = time_kernel(m, msl, M, N, K_dim, grid, tg)
        label = f"TC(arg={cand[2]})"
        print(f"{label:>40s}  {s['p50_ns']/1e3:>9.1f}  {s['p10_ns']/1e3:>9.1f}  "
              f"{s['gflops_p50']:>7.1f}  {'✓' if s['correct'] else '✗'}")
        rows.append((label, s))

        # Variant B: TC + GLOBAL on m + GLOBAL on n (parallel TC).
        kopts = [cand, (K.KOP_GLOBAL, 0, M), (K.KOP_GLOBAL, 1, N)]
        kid, root, msl = make_kernel(M, N, K_dim, kopts=kopts)
        grid, tg = dispatch_for(M, N, K_dim, parallel=True)
        s = time_kernel(m, msl, M, N, K_dim, grid, tg)
        label = f"TC(arg={cand[2]}) + parallel"
        print(f"{label:>40s}  {s['p50_ns']/1e3:>9.1f}  {s['p10_ns']/1e3:>9.1f}  "
              f"{s['gflops_p50']:>7.1f}  {'✓' if s['correct'] else '✗'}")
        rows.append((label, s))

    print()
    rows.sort(key=lambda r: r[1]["p50_ns"])
    print(f"WINNER: {rows[0][0]} -- {rows[0][1]['gflops_p50']:.1f} GFLOPS, "
          f"p50 {rows[0][1]['p50_ns']/1e3:.1f}us")
    return 0


if __name__ == "__main__":
    sys.exit(main())
