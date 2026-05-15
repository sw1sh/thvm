"""Score a raw MSL vector_add kernel against MLX `a + b`.

Reads kernel.metal + dispatch.json from this directory.
ABI (Metal.compile_msl -- you write the FULL kernel signature):
  buffer(0) = device       float *out  [N flat]
  buffer(1) = device const float *a    [N flat]
  buffer(2) = device const float *b    [N flat]
  N is injected as compile-time #define K_N (score.py prepends it).

dispatch.json picks grid + threadgroup:
  {"grid": [Gx,Gy,Gz], "threadgroup": [Tx,Ty,Tz]}        explicit
  {"rule": "<expr>", "grid_rule": "<expr>"}              shape-adaptive
rule exprs are eval'd with N, ceildiv, max, min, simd=32, tg in
scope; `rule` -> threadgroup total threads, `grid_rule` -> grid
total threads.

vector_add is memory-bandwidth bound -- the realistic target is
matching MLX, not beating it by a lot.  Headline metric is
speedup_gpu (GPU-time vs GPU-time); see py/examples/metaltime.py.

Usage:  ./score.sh [N]      (default 1048576)
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).parents[3]
sys.path.insert(0, str(ROOT))
from py.thvm import Metal
from py.examples.metaltime import (bench_candidate, bench_mlx_amortized,
                                   bench_mlx_wall, print_score)


def resolve_dispatch(cfg: dict, N: int):
    def ceildiv(a, b):
        return (a + b - 1) // b
    if "rule" in cfg:
        env = {"N": N, "ceildiv": ceildiv, "max": max, "min": min,
               "simd": 32}
        tg = int(eval(cfg["rule"], {"__builtins__": {}}, env))
        env["tg"] = tg
        grid_total = int(eval(cfg["grid_rule"], {"__builtins__": {}}, env))
        return (grid_total, 1, 1), (tg, 1, 1)
    return tuple(cfg["grid"]), tuple(cfg["threadgroup"])


def main() -> int:
    here = Path(__file__).parent
    N = int(sys.argv[1]) if len(sys.argv) >= 2 else 1048576

    src = (here / "kernel.metal").read_text()
    src = f"#define K_N {N}\n" + src
    cfg = json.loads((here / "dispatch.json").read_text())
    grid, tg = resolve_dispatch(cfg, N)

    m = Metal()
    try:
        pso = m.compile_msl(src, fn="k")
    except RuntimeError as e:
        print("status=compile_err")
        print(f"reason={str(e)[:300]}")
        return 1

    rng = np.random.default_rng(42)
    a = rng.uniform(-1, 1, N).astype(np.float32)
    b = rng.uniform(-1, 1, N).astype(np.float32)
    ref = a + b

    out_buf = m.buf_alloc(N * 4)
    a_buf = m.buf_alloc(N * 4)
    b_buf = m.buf_alloc(N * 4)
    m.buf_write_array(a_buf, a)
    m.buf_write_array(b_buf, b)
    bufs = [out_buf, a_buf, b_buf]

    try:
        for _ in range(3):
            m.dispatch(pso, bufs, grid=grid, threadgroup=tg)
    except RuntimeError as e:
        print("status=runtime_err")
        print(f"reason=warmup: {str(e)[:300]}")
        return 1

    got = m.buf_read_array(out_buf, (N,), np.float32)
    max_abs = float(np.max(np.abs(got - ref)))
    max_rel = float(np.max(np.abs(got - ref) / (np.abs(ref) + 1e-30)))
    if not (max_abs <= 1e-4 or max_rel <= 1e-3):
        print("status=correctness_err")
        print(f"correctness=max_abs:{max_abs:.3e} max_rel:{max_rel:.3e}")
        return 1

    cand_gpu, cand_wall = bench_candidate(m, pso, bufs, grid=grid,
                                          threadgroup=tg, reps=200)

    import mlx.core as mx
    # distinct input buffers so MLX cannot CSE the amortized batch
    a_in = [mx.array(rng.uniform(-1, 1, N).astype(np.float32))
            for _ in range(32)]
    b_mx = mx.array(b)
    mx.eval(b_mx, *a_in)
    mlx_amort = bench_mlx_amortized(lambda x: x + b_mx, a_in,
                                    reps=60, batch=32)

    a_mx = mx.array(a)
    mx.eval(a_mx)
    def fn():
        c = a_mx + b_mx
        mx.eval(c)
    mlx_wall = bench_mlx_wall(fn, reps=200)

    print_score(status="ok", max_abs=max_abs, max_rel=max_rel,
                cand_gpu=cand_gpu, cand_wall=cand_wall,
                mlx_amortized=mlx_amort, mlx_wall=mlx_wall)

    for h in bufs:
        m.buf_release(h)
    m.pso_release(pso)
    return 0


if __name__ == "__main__":
    sys.exit(main())
