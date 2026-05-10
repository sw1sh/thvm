"""Score a raw MSL layernorm kernel against mx.fast.layer_norm.

Reads kernel.metal + dispatch.json from this directory.

ABI:
  buffer(0) = device const float *x      [R*C flat]
  buffer(1) = device const float *gamma  [C]
  buffer(2) = device const float *beta   [C]
  buffer(3) = device       float *out    [R*C flat]
  buffer(4) = constant     int   &axis_size = C

dispatch.json: {"grid": [Gx, Gy, Gz], "threadgroup": [Tx, Ty, Tz]}
Both lists are MLX-style total threads. Optional "grid_per_row" =
[px, py, pz] expands to grid = [px*R, py, pz].

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


EPS = 1e-5


def main() -> int:
    here = Path(__file__).parent
    if len(sys.argv) >= 3:
        R, C = int(sys.argv[1]), int(sys.argv[2])
    else:
        R, C = 128, 768

    src = (here / "kernel.metal").read_text()
    cfg = json.loads((here / "dispatch.json").read_text())

    # Per-shape override: dispatch.json may have a "shapes" dict keyed by
    # "RxC" strings (e.g. "128x768") whose value is a sub-config; if the
    # current (R, C) matches one, that sub-config wins over the top-level.
    shape_key = f"{R}x{C}"
    if "shapes" in cfg and shape_key in cfg["shapes"]:
        cfg = {**cfg, **cfg["shapes"][shape_key]}

    # Optional macros: textual replacement of "#define <name> <old>" lines
    # with the per-shape value. Use cfg["defines"] = {"N_READS": 24, ...}.
    defines = cfg.get("defines", {})
    if defines:
        import re
        for name, val in defines.items():
            pattern = re.compile(rf"^#define\s+{re.escape(name)}\s+\S+", re.M)
            src = pattern.sub(f"#define {name} {val}", src)

    if "grid_per_row" in cfg:
        gpr = cfg["grid_per_row"]
        grid = (gpr[0] * R, gpr[1], gpr[2])
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
    gamma = np.ones(C, dtype=np.float32)
    beta = np.zeros(C, dtype=np.float32)

    # Reference: row-wise layernorm with eps=1e-5.
    mean = x.mean(axis=1, keepdims=True)
    var = ((x - mean) ** 2).mean(axis=1, keepdims=True)
    ref = ((x - mean) / np.sqrt(var + EPS) * gamma + beta).astype(np.float32)

    in_buf = m.buf_alloc(R * C * 4)
    gamma_buf = m.buf_alloc(C * 4)
    beta_buf = m.buf_alloc(C * 4)
    out_buf = m.buf_alloc(R * C * 4)
    axis_buf = m.buf_alloc(4)
    m.buf_write_array(in_buf, x)
    m.buf_write_array(gamma_buf, gamma)
    m.buf_write_array(beta_buf, beta)
    m.buf_write(axis_buf, struct.pack("<i", C))

    bufs = [in_buf, gamma_buf, beta_buf, out_buf, axis_buf]

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
        for b in bufs: m.buf_release(b)
        m.pso_release(pso)
        return 1

    samples = []
    for _ in range(30):
        wall, _gpu = m.dispatch_timed(pso, bufs, grid=grid, threadgroup=threadgroup)
        samples.append(wall)
    samples.sort()
    p50 = samples[15]; p10 = samples[max(0, 30 // 10 - 1)]

    import mlx.core as mx
    x_mx = mx.array(x); gamma_mx = mx.array(gamma); beta_mx = mx.array(beta)
    mx.eval(x_mx, gamma_mx, beta_mx)
    def fn():
        c = mx.fast.layer_norm(x_mx, gamma_mx, beta_mx, eps=EPS); mx.eval(c)
    for _ in range(3): fn()
    mlx_samples = []
    for _ in range(30):
        t0 = time.perf_counter_ns()
        fn()
        mlx_samples.append(time.perf_counter_ns() - t0)
    mlx_samples.sort()
    mlx_p50 = mlx_samples[15]; mlx_p10 = mlx_samples[max(0, 30 // 10 - 1)]

    speedup = mlx_p50 / p50 if p50 > 0 else 0
    print("status=ok")
    print(f"correctness=max_abs:{max_abs:.3e} max_rel:{max_rel:.3e}")
    print(f"candidate=p50:{p50/1e3:.1f}us p10:{p10/1e3:.1f}us")
    print(f"mlx_baseline=p50:{mlx_p50/1e3:.1f}us p10:{mlx_p10/1e3:.1f}us")
    print(f"speedup_vs_mlx={speedup:.3f}x")

    for b in bufs: m.buf_release(b)
    m.pso_release(pso)
    return 0


if __name__ == "__main__":
    sys.exit(main())
