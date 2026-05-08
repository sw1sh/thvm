"""Score harness for the softmax agent task.

Imports kernel.py from this directory and expects:
  build(h: Thvm, R, C) -> (root: Term, out_buf: Term, in_buf: Term)
  dispatch(R, C) -> dict(grid=..., threadgroup=...)

Compiles via thvm.Metal, runs against numpy + mlx baselines, prints
5-line machine-parseable output.
"""
from __future__ import annotations

import importlib.util
import sys
import time
from pathlib import Path

import numpy as np

ROOT = Path(__file__).parent.parent.parent.parent
sys.path.insert(0, str(ROOT))
from py.thvm import Thvm, Metal, K
from py.thvm.thvm import _lib  # for diagnostics if needed


def load_kernel(path):
    spec = importlib.util.spec_from_file_location("user_kernel", path)
    mod = importlib.util.module_from_spec(spec)
    assert spec.loader
    spec.loader.exec_module(mod)
    return mod


def main() -> int:
    here = Path(__file__).parent
    kernel_path = here / "kernel.py"
    if not kernel_path.exists():
        print("status=runtime_err")
        print("reason=missing_kernel_py")
        return 1
    kernel = load_kernel(kernel_path)

    R, C = 32, 256
    if len(sys.argv) >= 3:
        R, C = int(sys.argv[1]), int(sys.argv[2])

    h = Thvm()
    m = Metal()

    # Build the UOp DAG and render to MSL
    try:
        root, out_buf, in_buf = kernel.build(h, R, C)
        msl = h.render(root, name="k")
    except Exception as e:
        print("status=runtime_err")
        print(f"reason=build_or_render: {type(e).__name__}: {str(e)[:200]}")
        return 1

    # Compile MSL
    try:
        pso = m.compile_msl(msl, fn="k")
    except RuntimeError as e:
        print("status=compile_err")
        print(f"reason={str(e)[:300]}")
        print(f"--- generated MSL ---\n{msl}\n---")
        return 1

    # Dispatch shape
    try:
        grid_tg = kernel.dispatch(R, C)
    except Exception as e:
        print("status=runtime_err")
        print(f"reason=dispatch_fn: {type(e).__name__}: {str(e)[:200]}")
        return 1

    # Inputs
    rng = np.random.default_rng(42)
    x_np = rng.uniform(-3, 3, (R, C)).astype(np.float32)
    # numpy reference
    x_max = x_np.max(axis=1, keepdims=True)
    e = np.exp(x_np - x_max)
    ref = (e / e.sum(axis=1, keepdims=True)).astype(np.float32)

    # Allocate Metal buffers (renderer auto-discovers buffers from DAG;
    # convention: out is buffer(0), inputs follow in instance order)
    out_handle = m.buf_alloc(R * C * 4)
    in_handle = m.buf_alloc(R * C * 4)
    m.buf_write_array(in_handle, x_np)

    bufs = [out_handle, in_handle]

    # Warmup + correctness
    try:
        for _ in range(3):
            m.dispatch(pso, bufs, **grid_tg)
    except RuntimeError as e:
        print("status=runtime_err")
        print(f"reason=warmup: {str(e)[:300]}")
        print(f"--- generated MSL ---\n{msl}\n---")
        return 1

    got = m.buf_read_array(out_handle, (R, C), np.float32)
    max_abs = float(np.max(np.abs(got - ref)))
    max_rel = float(np.max(np.abs(got - ref) / (np.abs(ref) + 1e-30)))
    correct = (max_abs <= 1e-4) or (max_rel <= 1e-3)

    if not correct:
        print("status=correctness_err")
        print(f"correctness=max_abs:{max_abs:.3e} max_rel:{max_rel:.3e}")
        print(f"--- generated MSL ---\n{msl}\n---")
        m.buf_release(out_handle); m.buf_release(in_handle); m.pso_release(pso)
        return 1

    # Time candidate
    samples = []
    for _ in range(30):
        wall, _gpu = m.dispatch_timed(pso, bufs, **grid_tg)
        samples.append(wall)
    samples.sort()
    p10 = samples[max(0, 30 // 10 - 1)]
    p50 = samples[15]

    # Time MLX baseline
    import mlx.core as mx
    x_mx = mx.array(x_np)
    mx.eval(x_mx)
    def fn():
        c = mx.softmax(x_mx, axis=-1)
        mx.eval(c)
    for _ in range(3):
        fn()
    mlx_samples = []
    for _ in range(30):
        t0 = time.perf_counter_ns()
        fn()
        mlx_samples.append(time.perf_counter_ns() - t0)
    mlx_samples.sort()
    mlx_p50 = mlx_samples[15]
    mlx_p10 = mlx_samples[max(0, 30 // 10 - 1)]

    speedup = mlx_p50 / p50 if p50 > 0 else 0

    print("status=ok")
    print(f"correctness=max_abs:{max_abs:.3e} max_rel:{max_rel:.3e}")
    print(f"candidate=p50:{p50/1e3:.1f}us p10:{p10/1e3:.1f}us")
    print(f"mlx_baseline=p50:{mlx_p50/1e3:.1f}us p10:{mlx_p10/1e3:.1f}us")
    print(f"speedup_vs_mlx={speedup:.3f}x")

    m.buf_release(out_handle); m.buf_release(in_handle); m.pso_release(pso)
    return 0


if __name__ == "__main__":
    sys.exit(main())
