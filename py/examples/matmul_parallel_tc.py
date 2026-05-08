"""Demo parallel TC: m_axis + n_axis with KAX_GLOBAL drop the sgi==0
guard, emit position-bound code, dispatch with one SG per output tile.

Compares legacy guarded TC vs new parallel TC at the same shape.
"""
from __future__ import annotations

import sys
import time
from pathlib import Path

import numpy as np

ROOT = Path(__file__).parent.parent.parent
sys.path.insert(0, str(ROOT))

from py.thvm import Thvm, Metal, K


def build_matmul(h: Thvm, M: int, N: int, K_dim: int, *,
                 m_axis_type: int, n_axis_type: int):
    """Same matmul UOp as before, parameterised by axis_types on m & n."""
    out_buf = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(M, N), instance=0)
    a_buf = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(M, K_dim), instance=1)
    b_buf = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(K_dim, N), instance=2)

    m_axis = h.range(0, m_axis_type,    M)
    n_axis = h.range(1, n_axis_type,    N)
    k_axis = h.range(2, K.AXIS_REDUCE,  K_dim)

    K_c = h.iconst(K_dim)
    N_c = h.iconst(N)

    a_addr = h.iadd(h.imul(m_axis, K_c), k_axis)
    b_addr = h.iadd(h.imul(k_axis, N_c), n_axis)
    c_addr = h.iadd(h.imul(m_axis, N_c), n_axis)

    prod = h.mul(h.index_e(a_buf, a_addr), h.index_e(b_buf, b_addr))
    reduced = h.reduce(K.REDUCE_SUM, axis=2, src=prod)
    reduced = h.opt(reduced, K.OPT_TC, 0)
    return h.store(out_buf, c_addr, reduced)


def time_kernel(m: Metal, msl: str, M: int, N: int, K_dim: int,
                grid, threadgroup, reps: int = 30, warmup: int = 5):
    pso = m.compile_msl(msl, fn="k")
    rng = np.random.default_rng(42)
    a_np = rng.uniform(-1, 1, (M, K_dim)).astype(np.float32)
    b_np = rng.uniform(-1, 1, (K_dim, N)).astype(np.float32)
    c_ref = a_np @ b_np

    out_buf = m.buf_alloc(M * N * 4)
    a_buf   = m.buf_alloc(M * K_dim * 4)
    b_buf   = m.buf_alloc(K_dim * N * 4)
    m.buf_write_array(a_buf, a_np)
    m.buf_write_array(b_buf, b_np)

    for _ in range(warmup):
        m.dispatch(pso, [out_buf, a_buf, b_buf],
                   grid=grid, threadgroup=threadgroup)
    c_got = m.buf_read_array(out_buf, (M, N), np.float32)
    max_abs = float(np.max(np.abs(c_got - c_ref)))

    samples = []
    for _ in range(reps):
        wall, gpu = m.dispatch_timed(pso, [out_buf, a_buf, b_buf],
                                     grid=grid, threadgroup=threadgroup)
        samples.append((wall, gpu))
    samples.sort()
    p10 = samples[max(0, reps // 10 - 1)]
    p50 = samples[reps // 2]

    m.buf_release(out_buf); m.buf_release(a_buf); m.buf_release(b_buf)
    m.pso_release(pso)
    return dict(p10_wall_us=p10[0]/1e3, p10_gpu_us=p10[1]/1e3,
                p50_wall_us=p50[0]/1e3, p50_gpu_us=p50[1]/1e3,
                gflops=(2.0 * M * N * K_dim) / (p50[0] * 1e-9) / 1e9,
                max_abs=max_abs)


def main() -> int:
    h = Thvm()
    m = Metal()

    print(f"{'shape':>14}  {'mode':>14}  {'gflops':>7}  {'p50_wall':>10}  {'p50_gpu':>10}  {'p10_wall':>10}  correct")
    print("-" * 100)

    for M in [64, 128, 256, 512]:
        N = M
        K_dim = M
        n_tiles_n = N // 8
        n_tiles_m = M // 8
        n_tiles_total = n_tiles_n * n_tiles_m

        # ---- legacy guarded TC (m,n=LOOP) ----
        guarded_root = build_matmul(h, M, N, K_dim,
                                    m_axis_type=K.AXIS_LOOP,
                                    n_axis_type=K.AXIS_LOOP)
        msl_g = h.render(guarded_root, name="k")
        # Guarded -> only one SG runs; 32 threads, 1 TG
        s_g = time_kernel(m, msl_g, M, N, K_dim,
                          grid=(32, 1, 1), threadgroup=(32, 1, 1))
        print(f"{M}x{N}x{K_dim:>4d}    {'guarded':>14}  "
              f"{s_g['gflops']:>7.0f}  "
              f"{s_g['p50_wall_us']:>8.1f}us  {s_g['p50_gpu_us']:>8.1f}us  "
              f"{s_g['p10_wall_us']:>8.1f}us  {s_g['max_abs']:.1e}")

        # ---- parallel TC (m,n=GLOBAL) ----
        par_root = build_matmul(h, M, N, K_dim,
                                m_axis_type=K.AXIS_GLOBAL,
                                n_axis_type=K.AXIS_GLOBAL)
        msl_p = h.render(par_root, name="k")
        # Parallel: one TG per tile. n_tiles_total TGs. 32 threads each.
        s_p = time_kernel(m, msl_p, M, N, K_dim,
                          grid=(n_tiles_total * 32, 1, 1),
                          threadgroup=(32, 1, 1))
        print(f"{M}x{N}x{K_dim:>4d}    {'parallel TC':>14}  "
              f"{s_p['gflops']:>7.0f}  "
              f"{s_p['p50_wall_us']:>8.1f}us  {s_p['p50_gpu_us']:>8.1f}us  "
              f"{s_p['p10_wall_us']:>8.1f}us  {s_p['max_abs']:.1e}")

        # speedup
        if s_g['p50_wall_us'] > 0:
            print(f"{'':>14}  {'speedup p50':>14}: "
                  f"{s_g['p50_wall_us']/s_p['p50_wall_us']:>5.2f}x  "
                  f"({s_g['gflops']:.0f} -> {s_p['gflops']:.0f} GFLOPS)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
