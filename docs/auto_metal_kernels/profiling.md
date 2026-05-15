# Profiling: how to measure, what to compare against

The single biggest mistake an agent can make: declare victory based
on one measurement.  GPU timing on Mac has a noise floor of ~2x for
small kernels (sub-millisecond).  You need many samples + percentiles
to call a winner.

## The score harness contract

The current harnesses emit **eight lines** on stdout:

```
status=ok|compile_err|correctness_err|runtime_err
correctness=max_abs:X max_rel:Y
candidate_gpu=p50:Xus p10:Yus
candidate_wall=p50:Xus p10:Yus
mlx_amortized=p50:Xus p10:Yus
mlx_wall=p50:Xus p10:Yus
speedup_gpu=Kx
speedup_wall=Kx
```

Exit code: 0 if `status=ok`, 1 otherwise.  Parse these lines, log
them in `RESULTS.md`, iterate.

**`speedup_gpu` is the headline number.**  It compares GPU time to
GPU time (see below).  `speedup_wall` is the legacy wall-vs-wall
metric, kept as a diagnostic: when `speedup_gpu` and `speedup_wall`
disagree by a lot, the kernel is dispatch-bound -- trust
`speedup_gpu`.

The shared timing module is
`py/examples/metaltime.py`.  Per-op score harnesses
(e.g. `py/examples/agent_matmul/score.py`) import it; they differ
only in the reference computation, buffer layout, and MLX baseline.

The harness runs **200 candidate reps** and **60 amortized MLX
batches** after warmup.

## Why GPU timing -- the wall-time noise floor

Wall time = `time.perf_counter_ns()` around dispatch +
`waitUntilCompleted`.  It includes Python dispatch, encoder, queue,
and readback latency.  For sub-millisecond kernels that fixed
overhead is ~100-200us and swamps the actual kernel time -- a
~2x noise floor.  The 1.05x success threshold sits *below* that
noise.  Every agent in the May 2026 swarm hit this.

The fix, both halves in `metaltime.py`:

### Candidate: `gpu_ns` from `dispatch_timed`

```python
wall_ns, gpu_ns = m.dispatch_timed(pso, bufs, grid=..., threadgroup=...)
```

`gpu_ns` is the Metal command-buffer `GPUEndTime - GPUStartTime` --
the kernel's true GPU execution time, no Python, no encoder.
`bench_candidate` collects it over 200 reps.

### MLX: amortized eval

MLX exposes no per-op GPU timer.  `bench_mlx_amortized` recovers a
GPU-time-equivalent: build B (=32) distinct ops, then ONE `mx.eval`.
MLX pipelines all B kernels on the GPU back-to-back, so `wall/B`
converges to the GPU-bound per-op cost as B grows.  Distinct input
buffers defeat MLX's common-subexpression dedup.

`speedup_gpu = mlx_amortized_p50 / candidate_gpu_p50` -- GPU-time vs
GPU-time, the honest comparison.

## How many samples?

The harness uses 200 candidate reps + 40 amortized MLX batches.
With `gpu_ns` the per-sample noise is far lower than wall time, so
200 is plenty even for sub-100us kernels.  Still: run the whole
score 3 times and take the median speedup.

## Quiesce the machine -- GPU DVFS is real

Apple GPUs run dynamic voltage/frequency scaling.  The *same* kernel
measures very differently depending on the GPU's clock state, which
depends on what ran just before and on thermal headroom.  Observed:
a matmul kernel scored `44us` then `171us` on back-to-back harness
invocations with nothing changed -- a 4x swing from clock state and
contention alone.

Before trusting a number:

- Close other GPU clients (browsers with hardware accel, other
  bench processes, Metal apps).  Do not run two score harnesses
  concurrently.
- Warm up: the harness does 5 warmup dispatches, but a cold GPU may
  need more.  If run-to-run medians swing >20%, warm up by hand
  (run the score once, throw it away, run it 3 more times).
- Trust the *shape* of the result, not the absolute us.  If a kernel
  reports the same `candidate_gpu` for two very different problem
  sizes, the measurement is corrupt -- the GPU was in a different
  clock state, or contended.  Re-run on a quiet machine.

If 3 runs on a quiet machine give `speedup_gpu = 1.0x +/- 0.05x`,
the difference is below the floor -- declare a tie or change the
kernel materially.

## p10 vs p50

- **p50** (median): reflects "typical" performance.  `speedup_gpu`
  is computed from p50.
- **p10** (10th percentile, fastest 10%): reflects "best achievable".
  Useful for sanity-checking that you can hit a number even if you
  can't hit it consistently.

If your `candidate_gpu p10` beats MLX but `p50` doesn't, the kernel
is fast in principle but inconsistent -- usually a TG occupancy
issue, register spilling under contention, or grid shape mismatched
to GPU core count.

## Reading candidate_gpu vs candidate_wall

`candidate_wall - candidate_gpu` is your dispatch + sync overhead.
If they're within a few us, the kernel dominates and either number
is fine.  If `candidate_wall` is 2x `candidate_gpu`, the kernel is
fast but you're paying a lot to launch it -- the fix is fewer,
fatter dispatches (one big kernel, not many small ones), which
`speedup_gpu` already credits you for.

`mlx_wall - mlx_amortized` shows the same for MLX -- and it's
usually large (MLX pays Python + graph overhead per op).  That gap
is *not* something your kernel beats; `speedup_gpu` correctly
ignores it by comparing GPU time to GPU time.

## Correctness

```
correctness=max_abs:X max_rel:Y
```

Default thresholds in the per-op `score.py` harnesses:
- matmul / vector_add: `max_abs ≤ 1e-2` (matmul) / `1e-4`
  (vector_add) OR `max_rel ≤ 1e-3` -> pass
- softmax: `max_abs ≤ 1e-4` OR `max_rel ≤ 1e-3`

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
tinygrad with BEAM) and prints the comparison -- broader baseline
context than the single MLX number `score.sh` gives.

For the iteration loop, `./score.sh` in your `py/examples/agent_<op>/`
workspace is what you run -- it builds the MLX baseline into every
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
| run | candidate_gpu p50 | mlx_amortized p50 | speedup_gpu |
|---|---|---|---|
| 1 | 240us | 257us | 1.07x |
| 2 | 245us | 252us | 1.03x |
| 3 | 238us | 261us | 1.10x |

(median speedup_gpu: 1.07x, range: 1.03-1.10x)

## What surprised me about MLX
...

## What I'd try with more budget
...
```

The variance row matters.  If a single run shows 1.5x but three runs
show 0.9-1.5x, the honest answer is "noisy, sometimes wins, often
ties".
