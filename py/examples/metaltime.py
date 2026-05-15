"""Shared Metal timing helpers -- GPU-timestamp + amortized-MLX.

Why this exists: the score harnesses originally compared candidate
*wall time* against MLX *wall time*.  For sub-millisecond kernels
wall time is dominated by Python dispatch + encoder overhead, which
has a ~2x noise floor -- the 1.05x success threshold sits *below*
the noise.  Every agent in the May 2026 swarm hit this.

Two fixes, both here:

1. Candidate: `dispatch_timed` already returns `gpu_ns` (the Metal
   command-buffer `GPUEndTime - GPUStartTime`).  That is the true
   kernel execution time -- no Python, no encoder.  `bench_candidate`
   collects it.

2. MLX: MLX exposes no per-op GPU timer.  `bench_mlx_amortized`
   recovers a GPU-time-equivalent by amortization: build B distinct
   ops, then ONE `mx.eval` -- MLX pipelines all B kernels on the GPU
   back-to-back, so wall/B converges to the GPU-bound per-op time as
   B grows.  Distinct input buffers defeat MLX's common-subexpression
   dedup.

The scored speedup is then `mlx_amortized_p50 / candidate_gpu_p50` --
GPU-time vs GPU-time.  Wall numbers are still reported as diagnostics:
when wall and GPU speedups disagree, the kernel is dispatch-bound and
the GPU number is the one to trust.
"""
from __future__ import annotations

import time
from dataclasses import dataclass


@dataclass
class Timing:
    """p10/p50 in nanoseconds for one measured thing."""
    p10_ns: float
    p50_ns: float

    @classmethod
    def from_samples(cls, samples: list[float]) -> "Timing":
        s = sorted(samples)
        n = len(s)
        return cls(p10_ns=s[max(0, n // 10 - 1)], p50_ns=s[n // 2])

    def us(self, which: str = "p50") -> float:
        return (self.p50_ns if which == "p50" else self.p10_ns) / 1e3


def bench_candidate(m, pso, bufs, *, grid, threadgroup,
                    reps: int = 200, warmup: int = 5):
    """Time a thvm-compiled PSO via Metal command-buffer timestamps.

    Returns (gpu: Timing, wall: Timing).  `gpu` is the kernel's true
    GPU execution time -- use it as the candidate's headline number.
    `wall` includes our dispatch + waitUntilCompleted overhead.
    """
    for _ in range(warmup):
        m.dispatch_timed(pso, bufs, grid=grid, threadgroup=threadgroup)
    walls: list[float] = []
    gpus: list[float] = []
    for _ in range(reps):
        wall, gpu = m.dispatch_timed(pso, bufs, grid=grid,
                                     threadgroup=threadgroup)
        walls.append(float(wall))
        gpus.append(float(gpu))
    return Timing.from_samples(gpus), Timing.from_samples(walls)


def bench_mlx_amortized(mlx_op, make_batch, *, reps: int = 40,
                        batch: int = 16, warmup: int = 4):
    """Amortized per-op time for an MLX op.

    `make_batch()` must return a FRESH list of `batch` distinct,
    already-evaluated mx.array inputs every time it is called -- fresh
    data each rep is essential: MLX memoizes evaluated arrays, so if
    the same input objects are reused, every rep after the first is a
    free cache hit and the measurement collapses to ~0 (the bug this
    signature replaces).

    `mlx_op(x)` returns one unevaluated mx.array.  Each rep builds
    `batch` ops over the fresh inputs, then one `mx.eval`; per-op time
    is wall/batch.  Python's per-call overhead is paid once per batch,
    not once per op, so as `batch` grows per-op converges to MLX's
    GPU-bound cost.

    Returns a Timing over `reps` per-op samples.
    """
    import mlx.core as mx

    def one_batch() -> float:
        ins = make_batch()
        mx.eval(*ins)                       # inputs ready, not timed
        outs = [mlx_op(x) for x in ins]
        t0 = time.perf_counter_ns()
        mx.eval(*outs)
        return (time.perf_counter_ns() - t0) / batch

    for _ in range(warmup):
        one_batch()
    return Timing.from_samples([one_batch() for _ in range(reps)])


def bench_mlx_wall(fn, *, reps: int = 200, warmup: int = 5):
    """Single-op MLX wall time (the legacy metric).

    `fn()` must run the op AND synchronize (call mx.eval) before
    returning.  Kept for continuity / as a diagnostic next to the
    amortized number.
    """
    for _ in range(warmup):
        fn()
    samples: list[float] = []
    for _ in range(reps):
        t0 = time.perf_counter_ns()
        fn()
        samples.append(float(time.perf_counter_ns() - t0))
    return Timing.from_samples(samples)


def print_score(*, status: str, max_abs: float, max_rel: float,
                cand_gpu: Timing, cand_wall: Timing,
                mlx_amortized: Timing, mlx_wall: Timing) -> None:
    """Emit the machine-parseable score block.

    Contract (8 lines for status=ok):
      status=ok
      correctness=max_abs:X max_rel:Y
      candidate_gpu=p50:Xus p10:Yus
      candidate_wall=p50:Xus p10:Yus
      mlx_amortized=p50:Xus p10:Yus
      mlx_wall=p50:Xus p10:Yus
      speedup_gpu=Kx
      speedup_wall=Kx

    speedup_gpu is the headline (GPU-time vs GPU-time).  speedup_wall
    is the legacy wall-vs-wall.  When they disagree by a lot the
    kernel is dispatch-bound -- trust speedup_gpu.
    """
    print(f"status={status}")
    print(f"correctness=max_abs:{max_abs:.3e} max_rel:{max_rel:.3e}")
    print(f"candidate_gpu=p50:{cand_gpu.us():.1f}us p10:{cand_gpu.us('p10'):.1f}us")
    print(f"candidate_wall=p50:{cand_wall.us():.1f}us p10:{cand_wall.us('p10'):.1f}us")
    print(f"mlx_amortized=p50:{mlx_amortized.us():.1f}us p10:{mlx_amortized.us('p10'):.1f}us")
    print(f"mlx_wall=p50:{mlx_wall.us():.1f}us p10:{mlx_wall.us('p10'):.1f}us")
    sg = mlx_amortized.p50_ns / cand_gpu.p50_ns if cand_gpu.p50_ns else 0.0
    sw = mlx_wall.p50_ns / cand_wall.p50_ns if cand_wall.p50_ns else 0.0
    print(f"speedup_gpu={sg:.3f}x")
    print(f"speedup_wall={sw:.3f}x")
