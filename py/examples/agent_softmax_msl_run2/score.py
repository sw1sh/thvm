"""Score a raw MSL softmax kernel against mx.softmax.

Reads kernel.metal + dispatch.json from this directory.
ABI:
  buffer(0) = device const float *in    [R*C flat]
  buffer(1) = device       float *out   [R*C flat]
  buffer(2) = constant     int   &axis_size  = C

dispatch.json: {"grid": [Gx, Gy, Gz], "threadgroup": [Tx, Ty, Tz]}
Both lists are MLX-style total threads; if you want grid scaling
with R or C, write the dispatch.json with explicit numbers per
shape (or just use the largest you ever need; score.py won't
multi-shape-resolve symbolic).

Optional: dispatch.json can also have "grid_per_row": [px, py, pz]
which means grid = [px*R, py, pz] — convenient since most softmax
kernels run one threadgroup per row.

Output 5 lines:
  status=ok|compile_err|correctness_err|runtime_err
  correctness=max_abs:X max_rel:Y
  candidate=p50:Xus p10:Yus
  mlx_baseline=p50:Xus p10:Yus
  speedup_vs_mlx=Kx
"""
from __future__ import annotations

import json
import sys
import time
import struct
from pathlib import Path

import numpy as np

ROOT = Path(__file__).parent.parent.parent.parent
sys.path.insert(0, str(ROOT))
from py.thvm import Metal


def main() -> int:
    here = Path(__file__).parent
    if len(sys.argv) >= 3:
        R, C = int(sys.argv[1]), int(sys.argv[2])
    else:
        R, C = 32, 256

    src = (here / "kernel.metal").read_text()
    cfg = json.loads((here / "dispatch.json").read_text())
    # Inject shape as compile-time constants so kernel can specialize.
    src = f"#define K_C {C}\n#define K_R {R}\n" + src

    # Optional per-shape rule: pick (tg, grid_total) from (R, C).
    # Available names in eval: R, C, ceildiv, max, min, simd=32.
    if "rule" in cfg:
        def ceildiv(a, b):
            return (a + b - 1) // b
        env = {"R": R, "C": C, "ceildiv": ceildiv, "max": max, "min": min, "simd": 32}
        tg = int(eval(cfg["rule"], {"__builtins__": {}}, env))
        # Optional separate grid rule; default = one TG per row.
        if "grid_rule" in cfg:
            env["tg"] = tg
            grid_total = int(eval(cfg["grid_rule"], {"__builtins__": {}}, env))
        else:
            grid_total = tg * R
        grid = (grid_total, 1, 1)
        threadgroup = (tg, 1, 1)
    elif "grid_per_row" in cfg:
        gpr = cfg["grid_per_row"]
        grid = (gpr[0] * R, gpr[1], gpr[2])
        threadgroup = tuple(cfg["threadgroup"])
    else:
        grid = tuple(cfg["grid"])
        threadgroup = tuple(cfg["threadgroup"])

    m = Metal()
    try:
        pso = m.compile_msl(src, fn="k")
    except RuntimeError as e:
        print("status=compile_err")
        print(f"reason={str(e)[:300]}")
        return 1

    rng = np.random.default_rng(42)
    x = rng.uniform(-3, 3, (R, C)).astype(np.float32)
    x_max = x.max(axis=1, keepdims=True)
    e = np.exp(x - x_max)
    ref = (e / e.sum(axis=1, keepdims=True)).astype(np.float32)

    in_buf = m.buf_alloc(R * C * 4)
    out_buf = m.buf_alloc(R * C * 4)
    axis_buf = m.buf_alloc(4)
    m.buf_write_array(in_buf, x)
    m.buf_write(axis_buf, struct.pack("<i", C))

    bufs = [in_buf, out_buf, axis_buf]

    try:
        for _ in range(3):
            m.dispatch(pso, bufs, grid=grid, threadgroup=threadgroup)
    except RuntimeError as e:
        print("status=runtime_err")
        print(f"reason=warmup: {str(e)[:300]}")
        return 1

    got = m.buf_read_array(out_buf, (R, C), np.float32)
    max_abs = float(np.max(np.abs(got - ref)))
    max_rel = float(np.max(np.abs(got - ref) / (np.abs(ref) + 1e-30)))
    correct = (max_abs <= 1e-4) or (max_rel <= 1e-3)

    if not correct:
        print("status=correctness_err")
        print(f"correctness=max_abs:{max_abs:.3e} max_rel:{max_rel:.3e}")
        m.buf_release(in_buf); m.buf_release(out_buf); m.buf_release(axis_buf)
        m.pso_release(pso)
        return 1

    N_SAMPLES = 100
    samples = []
    for _ in range(N_SAMPLES):
        wall, _gpu = m.dispatch_timed(pso, bufs, grid=grid, threadgroup=threadgroup)
        samples.append(wall)
    samples.sort()
    p50 = samples[N_SAMPLES // 2]; p10 = samples[max(0, N_SAMPLES // 10 - 1)]

    import mlx.core as mx
    x_mx = mx.array(x); mx.eval(x_mx)
    def fn():
        c = mx.softmax(x_mx, axis=-1); mx.eval(c)
    for _ in range(3): fn()
    mlx_samples = []
    for _ in range(N_SAMPLES):
        t0 = time.perf_counter_ns()
        fn()
        mlx_samples.append(time.perf_counter_ns() - t0)
    mlx_samples.sort()
    mlx_p50 = mlx_samples[N_SAMPLES // 2]; mlx_p10 = mlx_samples[max(0, N_SAMPLES // 10 - 1)]

    speedup = mlx_p50 / p50 if p50 > 0 else 0
    print("status=ok")
    print(f"correctness=max_abs:{max_abs:.3e} max_rel:{max_rel:.3e}")
    print(f"candidate=p50:{p50/1e3:.1f}us p10:{p10/1e3:.1f}us")
    print(f"mlx_baseline=p50:{mlx_p50/1e3:.1f}us p10:{mlx_p10/1e3:.1f}us")
    print(f"speedup_vs_mlx={speedup:.3f}x")

    m.buf_release(in_buf); m.buf_release(out_buf); m.buf_release(axis_buf)
    m.pso_release(pso)
    return 0


if __name__ == "__main__":
    sys.exit(main())
