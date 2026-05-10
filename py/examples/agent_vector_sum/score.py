"""Score a raw MSL vector_sum kernel against mx.sum.

Reads kernel.metal + dispatch.json from this directory.

Two dispatch flavors:

A. Single-pass + atomic add (one kernel, function name "k"):
   ABI:
     buffer(0) = device const float *in   [N flat]
     buffer(1) = device atomic_uint *out  [1] (treated as fp32 atomic add)
     buffer(2) = constant int &n          input length
   dispatch.json: {"lsize": 1024, "n_reads": 4, "tile_size_per_tg_mul": 8}
     -> num_tgs = clamp(N / (lsize*n_reads*tile_mul), 1, 4096)

B. Two-pass (two kernels, "k1" then "k2"):
   ABI(k1):
     buffer(0) = in     [N]
     buffer(1) = part   [n_tgs]   intermediate per-TG partials
     buffer(2) = constant int &n
   ABI(k2):
     buffer(0) = part   [n_tgs]
     buffer(1) = out    [1]
     buffer(2) = constant int &n_partials
   dispatch.json: {"two_pass": true, "lsize1": 1024, "n_reads": 4,
                   "n_tgs": 128, "lsize2": 32}

Output 5 lines:
  status=ok|compile_err|correctness_err|runtime_err
  correctness=abs:X rel:Y (single value)
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
    if len(sys.argv) >= 2:
        N = int(sys.argv[1])
    else:
        N = 65_536

    src = (here / "kernel.metal").read_text()
    cfg = json.loads((here / "dispatch.json").read_text())

    m = Metal()
    two_pass = bool(cfg.get("two_pass", False))

    try:
        if two_pass:
            pso1 = m.compile_msl(src, fn="k1")
            pso2 = m.compile_msl(src, fn="k2")
        else:
            pso = m.compile_msl(src, fn="k")
    except RuntimeError as e:
        print("status=compile_err")
        print(f"reason={str(e)[:300]}")
        return 1

    rng = np.random.default_rng(42)
    x = rng.uniform(-1.0, 1.0, (N,)).astype(np.float32)
    ref = float(x.sum(dtype=np.float64))  # high-precision reference

    in_buf = m.buf_alloc(N * 4)
    out_buf = m.buf_alloc(4)
    n_buf = m.buf_alloc(4)
    m.buf_write_array(in_buf, x)
    m.buf_write(n_buf, struct.pack("<i", N))

    if two_pass:
        lsize1 = int(cfg.get("lsize1", 1024))
        n_reads = int(cfg.get("n_reads", 4))
        n_tgs = int(cfg.get("n_tgs", 128))
        lsize2 = int(cfg.get("lsize2", 32))
        part_buf = m.buf_alloc(n_tgs * 4)
        np_buf = m.buf_alloc(4)
        m.buf_write(np_buf, struct.pack("<i", n_tgs))

        # Reset out + part for warmup.
        bufs1 = [in_buf, part_buf, n_buf]
        bufs2 = [part_buf, out_buf, np_buf]

        def reset_out():
            m.buf_write(out_buf, struct.pack("<f", 0.0))

        def run_once():
            reset_out()
            m.dispatch(pso1, bufs1, grid=(lsize1 * n_tgs, 1, 1),
                       threadgroup=(lsize1, 1, 1))
            m.dispatch(pso2, bufs2, grid=(lsize2, 1, 1),
                       threadgroup=(lsize2, 1, 1))

        def run_once_timed():
            reset_out()
            t0 = time.perf_counter_ns()
            m.dispatch(pso1, bufs1, grid=(lsize1 * n_tgs, 1, 1),
                       threadgroup=(lsize1, 1, 1))
            wall, _ = m.dispatch_timed(pso2, bufs2, grid=(lsize2, 1, 1),
                                        threadgroup=(lsize2, 1, 1))
            return time.perf_counter_ns() - t0
    else:
        lsize = int(cfg.get("lsize", 1024))
        n_reads = int(cfg.get("n_reads", 4))
        if "num_tgs_fixed" in cfg:
            num_tgs = max(1, min(int(cfg["num_tgs_fixed"]),
                                 (N + lsize - 1) // lsize))
        elif "num_tgs_min_work" in cfg:
            min_work = int(cfg["num_tgs_min_work"])
            cap = int(cfg.get("num_tgs_cap", 128))
            num_tgs = max(1, min(cap, (N + min_work - 1) // min_work))
        else:
            tile_mul = int(cfg.get("tile_size_per_tg_mul", 8))
            per_tg = lsize * n_reads * tile_mul
            num_tgs = max(1, min(4096, (N + per_tg - 1) // per_tg))
        # Cooperative two-stage needs scratch + counter buffers.
        part_buf = m.buf_alloc(num_tgs * 4)
        counter_buf = m.buf_alloc(4)
        m.buf_write(counter_buf, struct.pack("<I", 0))
        bufs = [in_buf, out_buf, n_buf, part_buf, counter_buf]

        def run_once():
            m.dispatch(pso, bufs, grid=(lsize * num_tgs, 1, 1),
                       threadgroup=(lsize, 1, 1))

        def run_once_timed():
            wall, _ = m.dispatch_timed(pso, bufs, grid=(lsize * num_tgs, 1, 1),
                                        threadgroup=(lsize, 1, 1))
            return wall

    try:
        for _ in range(3):
            run_once()
    except RuntimeError as e:
        print("status=runtime_err")
        print(f"reason=warmup: {str(e)[:300]}")
        return 1

    # Force a final dispatch to flush state and read.
    run_once()
    # For two_pass, dispatch was async; force sync via dispatch_timed of a no-op? Use a final read which is synced via dispatch_timed wrapper.
    # Actually m.dispatch returns immediately. Use dispatch_timed's waitUntilCompleted.
    # Simplest: use timed wrapper for the last step.
    if two_pass:
        m.buf_write(out_buf, struct.pack("<f", 0.0))
        m.dispatch(pso1, bufs1, grid=(lsize1 * n_tgs, 1, 1),
                   threadgroup=(lsize1, 1, 1))
        m.dispatch_timed(pso2, bufs2, grid=(lsize2, 1, 1),
                          threadgroup=(lsize2, 1, 1))
    else:
        m.dispatch_timed(pso, bufs, grid=(lsize * num_tgs, 1, 1),
                          threadgroup=(lsize, 1, 1))

    got = m.buf_read_array(out_buf, (1,), np.float32)
    got_v = float(got[0])
    abs_err = abs(got_v - ref)
    rel_err = abs_err / (abs(ref) + 1e-30)
    # Match MLX-style tolerance: ~5e-4 relative or ~1e-3 absolute on large reductions.
    correct = (abs_err <= 1e-2 * max(1.0, abs(ref) * 1e-3 + 1.0)) or (rel_err <= 5e-4)
    # Tighter: explicit thresholds matching reference.py rtol=5e-4 atol=1e-3 (scale-aware).
    correct = (abs_err <= max(1e-3, 5e-4 * abs(ref)))

    if not correct:
        print("status=correctness_err")
        print(f"correctness=abs:{abs_err:.3e} rel:{rel_err:.3e} got:{got_v:.6f} ref:{ref:.6f}")
        return 1

    samples = []
    for _ in range(30):
        wall = run_once_timed()
        samples.append(wall)
    samples.sort()
    p50 = samples[15]; p10 = samples[max(0, 30 // 10 - 1)]

    import mlx.core as mx
    x_mx = mx.array(x); mx.eval(x_mx)
    def fn():
        c = mx.sum(x_mx); mx.eval(c)
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
    print(f"correctness=abs:{abs_err:.3e} rel:{rel_err:.3e}")
    print(f"candidate=p50:{p50/1e3:.1f}us p10:{p10/1e3:.1f}us")
    print(f"mlx_baseline=p50:{mlx_p50/1e3:.1f}us p10:{mlx_p10/1e3:.1f}us")
    print(f"speedup_vs_mlx={speedup:.3f}x")

    m.buf_release(in_buf); m.buf_release(out_buf); m.buf_release(n_buf)
    if two_pass:
        m.buf_release(part_buf); m.buf_release(np_buf)
        m.pso_release(pso1); m.pso_release(pso2)
    else:
        m.buf_release(part_buf); m.buf_release(counter_buf)
        m.pso_release(pso)
    return 0


if __name__ == "__main__":
    sys.exit(main())
