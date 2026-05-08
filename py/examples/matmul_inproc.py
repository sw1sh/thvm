"""End-to-end matmul: thvm-py builds UOp DAG, renders MSL, compiles via
in-process Metal (no xcrun subprocess), dispatches, verifies vs numpy.

Compare wall time to matmul_e2e.py (subprocess path) to measure the
overhead saved.
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


def run_one(M: int, N: int, K_dim: int, *, with_tc: bool, reps: int = 20,
            warmup: int = 3) -> dict:
    """Build + compile + dispatch a matmul; return stats."""
    h = Thvm()
    m = Metal()

    t0 = time.perf_counter_ns()
    root = build_matmul(h, M, N, K_dim, with_tc=with_tc)
    msl = h.render(root, name="k")
    t_render = time.perf_counter_ns() - t0

    t0 = time.perf_counter_ns()
    pso = m.compile_msl(msl, fn="k")
    t_compile = time.perf_counter_ns() - t0

    # Set up inputs
    rng = np.random.default_rng(42)
    a_np = rng.uniform(-1, 1, (M, K_dim)).astype(np.float32)
    b_np = rng.uniform(-1, 1, (K_dim, N)).astype(np.float32)
    c_ref = a_np @ b_np

    # The renderer's auto-discovered buffer order is:
    #   buffer(0) = out, buffer(1) = a (instance=1), buffer(2) = b (instance=2)
    out_buf = m.buf_alloc(M * N * 4)
    a_buf   = m.buf_alloc(M * K_dim * 4)
    b_buf   = m.buf_alloc(K_dim * N * 4)
    m.buf_write_array(a_buf, a_np)
    m.buf_write_array(b_buf, b_np)

    # Dispatch shape: plain LOOP-only kernel needs 1 thread; OPT(_, TC)
    # needs 1 simdgroup (32 threads) since the renderer wraps with
    # `if (sgi == 0u && tg == 0u)`.
    if with_tc:
        grid = (32, 1, 1); threadgroup = (32, 1, 1)
    else:
        grid = (1, 1, 1);  threadgroup = (1, 1, 1)

    # Warmup + correctness
    for _ in range(warmup):
        m.dispatch(pso, [out_buf, a_buf, b_buf],
                   grid=grid, threadgroup=threadgroup)
    c_got = m.buf_read_array(out_buf, (M, N), np.float32)
    max_abs = float(np.max(np.abs(c_got - c_ref)))
    max_rel = float(np.max(np.abs(c_got - c_ref) / (np.abs(c_ref) + 1e-30)))

    # Time reps
    samples = []
    for _ in range(reps):
        wall, gpu = m.dispatch_timed(pso, [out_buf, a_buf, b_buf],
                                     grid=grid, threadgroup=threadgroup)
        samples.append((wall, gpu))
    samples.sort()
    p10 = samples[max(0, reps // 10 - 1)]
    p50 = samples[reps // 2]

    # Cleanup
    m.buf_release(out_buf); m.buf_release(a_buf); m.buf_release(b_buf)
    m.pso_release(pso)

    return dict(
        M=M, N=N, K=K_dim, with_tc=with_tc,
        render_us=t_render / 1e3,
        compile_us=t_compile / 1e3,
        max_abs=max_abs, max_rel=max_rel,
        p10_wall_us=p10[0] / 1e3, p10_gpu_us=p10[1] / 1e3,
        p50_wall_us=p50[0] / 1e3, p50_gpu_us=p50[1] / 1e3,
        gflops_p50=(2.0 * M * N * K_dim) / (p50[0] * 1e-9) / 1e9 if p50[0] else 0,
    )


def fmt(d: dict) -> str:
    return (
        f"shape={d['M']}x{d['N']}x{d['K']:>4d} tc={int(d['with_tc'])}  "
        f"render={d['render_us']:7.1f}us  compile={d['compile_us']:8.1f}us  "
        f"p50_wall={d['p50_wall_us']:7.1f}us  p50_gpu={d['p50_gpu_us']:7.1f}us  "
        f"gflops={d['gflops_p50']:5.0f}  "
        f"correct(abs={d['max_abs']:.2e}/rel={d['max_rel']:.2e})"
    )


def main() -> int:
    print("In-process Metal: thvm renders -> Metal compiles -> dispatch.")
    print("=" * 100)
    for M, N, K_dim in [(64, 64, 64), (128, 128, 128)]:
        print(fmt(run_one(M, N, K_dim, with_tc=False)))
        print(fmt(run_one(M, N, K_dim, with_tc=True)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
