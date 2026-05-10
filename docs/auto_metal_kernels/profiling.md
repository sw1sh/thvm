# Profiling: how to measure, what to compare against

The single biggest mistake an agent can make: declare victory based
on one measurement.  GPU timing on Mac has a noise floor of ~2x for
small kernels (sub-millisecond).  You need many samples + percentiles
to call a winner.

## The score harness contract

All score harnesses emit five lines on stdout:

```
status=ok|compile_err|correctness_err|runtime_err
correctness=max_abs:X max_rel:Y
candidate=p50:Xus p10:Yus
mlx_baseline=p50:Xus p10:Yus
speedup_vs_mlx=Kx
```

Exit code: 0 if `status=ok`, 1 otherwise.  Parse these lines, log
them in `RESULTS.md`, iterate.

The two reference harnesses:

- `bench/metal-problems/runner/score_one.py` -- agent kernel as
  Python module (`SOURCE` + `dispatch()`); MLX baseline is `a @ b`,
  `mx.softmax(x)`, etc. depending on the op
- `py/examples/agent_softmax_msl/score.sh` -- agent kernel as
  `kernel.metal` + `dispatch.json`; MLX baseline is `mx.softmax`

Both run **30 reps after 3 warmup**; report **p10 and p50** wall-time.

## `Metal.dispatch_timed`

```python
wall_ns, gpu_ns = m.dispatch_timed(pso, bufs, grid=..., threadgroup=...)
```

Returns two timings:

- `wall_ns`: `time.perf_counter_ns()` around the dispatch +
  `[cmdBuffer waitUntilCompleted]`.  Includes encoder + queue +
  GPU + readback latency.  **This is what the score harnesses use.**
- `gpu_ns`: derived from `MTLCommandBuffer.GPUStartTime` / `.GPUEndTime`.
  Pure GPU execution time, no encoder/queue overhead.  More precise
  for small kernels.

For sub-100us kernels, **`wall_ns` is dominated by encoder cost**
and the variance is high.  `gpu_ns` is the better signal but the
score harnesses don't (yet) use it -- if you're chasing a sub-5%
improvement, you'll need the GPU timing path.

## How many samples?

The harness uses 30 samples + p50.  This is the minimum for
sub-millisecond kernels.  For a tight comparison (1.05x threshold):

| Kernel runtime | Samples needed for 1.05x detection |
|---|---|
| > 10 ms | 5-10 |
| 1-10 ms | 20-50 |
| 100us-1ms | 50-200 |
| < 100us | 200+ (or use `gpu_ns`) |

If 30 samples give you `1.0x +/- 0.2x`, run more.  If 100 samples
still give `1.05x +/- 0.1x`, the difference is below your noise
floor -- declare it a tie or change the kernel materially.

## p10 vs p50

- **p50** (median): reflects "typical" performance.  The score
  harness compares this.
- **p10** (10th percentile, fastest 10%): reflects "best achievable".
  Useful for sanity-checking that you can hit a number even if you
  can't hit it consistently.

If your `cand p10` beats MLX p50 but your `cand p50` doesn't, the
kernel is fast in principle but inconsistent -- usually a TG
occupancy issue, register spilling under contention, or grid shape
mismatched to GPU core count.

## Correctness

```
correctness=max_abs:X max_rel:Y
```

Default thresholds in `score_one.py`:
- `max_abs ≤ 1e-3` OR `max_rel ≤ 1e-3` -> pass

Tighter for softmax (`score.py`): `max_abs ≤ 1e-4` OR `max_rel ≤ 1e-3`.

Reference is computed in NumPy (FP32 matmul / softmax / layernorm).
For workloads where bit-exact match isn't possible (e.g. if you reorder
reductions for parallelism), the relative-error path catches you --
~1e-3 is roughly fp32 round-off accumulated over 1024-1M ops.

If `max_abs` blows up to `inf` or `nan`, you've got a numerical bug
(divide-by-zero, OOB accumulation, `-INFINITY` propagation -- see
[pitfalls.md](pitfalls.md)).

## Headline benchmarks: what's been measured

From `bench/metal-problems/matmul/RESULTS.md` on Apple M3 Max:

### L = 1024 matmul

| Tool | p50 (ms) | GFLOPS | % of FP32 peak (14.2 TFLOPS) |
|---|---:|---:|---:|
| pytorch_mps_eager | 0.602 | 3567 | 25% |
| mlx_eager | 0.684 | 3142 | 22% |
| **mlx_compile** | **0.571** | **3763** | **27%** |
| tinygrad_metal_beam4 | 1.167 | 1840 | 13% |
| msl_stage4_tiled_32x32x32 | 0.745 | 2885 | 20% |

### L = 2048 matmul

| Tool | p50 (ms) | GFLOPS | % of peak |
|---|---:|---:|---:|
| pytorch_mps_eager | 2.233 | 7692 | 54% |
| **mlx_eager** | **1.830** | **9390** | **66%** |
| msl_stage4_tiled_32x32x32 | 6.582 | 2610 | 18% |

**MLX hits 66% of FP32 peak at L=2048.  That's the bar.**  At L=512
everyone is dispatch-bound (~50-100us of encoder overhead per
dispatch).  At L=2048 the GPU is the bottleneck and MLX wins.

### Softmax (R=4096, C=4096)

From prior `agent_softmax_msl` run:

| Variant | speedup vs `mx.softmax` |
|---|---:|
| Naive (1 thread per row) | 0.19x |
| MLX-style `softmax_single_row` (N_READS=8, lsize=512) | ~1.0-1.3x (depending on run) |

Variance is ~30% between runs at this shape -- the `1.05x` threshold
is below the wall-time noise floor without GPU timing.

## How to compare against PyTorch MPS / MLX / tinygrad

The `bench/metal-problems/runner/sweep.py` driver runs all four
(your kernel, MLX eager, MLX compile, PyTorch MPS, optionally
tinygrad with BEAM) and prints the comparison.  Useful for
"speedup_vs_mlx" framing.

For one-off scoring, use `score_one.py` (Python kernel module) or
`score.sh` (raw MSL).  Both already run MLX baseline as part of the
score.

## What to write in `RESULTS.md`

Per-iteration log + final report.  Template:

```markdown
# <Op> -- <agent name> results

## Iteration log

| iter | change | shape A speedup | shape B speedup |
|---|---|---:|---:|
| 0 | naive baseline | 0.85x | 0.20x |
| 1 | + N_READS=8 vectorize | 1.34x | 0.86x |
| 2 | + simd_sum reduce | 2.12x | 2.04x |
| ... |

## Final kernel

(See kernel.metal in this dir.)

## Final dispatch.json

```json
{...}
```

## Final 3-run variance

Shape A:
| run | cand p50 | mlx p50 | speedup |
|---|---|---|---|
| 1 | 240us | 257us | 1.07x |
| 2 | 265us | 240us | 0.91x |
| 3 | 207us | 257us | 1.24x |

(median speedup: 1.07x, range: 0.91-1.24x)

## What surprised me about MLX
...

## What I'd try with more budget
...
```

The variance row matters.  If a single run shows 1.5x but three runs
show 0.9-1.5x, the honest answer is "noisy, sometimes wins, often
ties".
