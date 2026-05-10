# softmax via raw MSL — agent task: BEAT MLX

Goal: write a raw Metal Shading Language softmax kernel that beats
`mx.softmax` at (R=32, C=256) and (R=4096, C=4096) on Apple M3 Max.

Previous agent run via thvm UOp DAG hit ~0.92× MLX. This run is
pure MSL — no UOp DAG, no thvm IR. You write MSL strings, our
`Metal.compile_msl` compiles in-process, dispatches, scores.

The reason MLX is hard to beat: it uses `simd_max` / `simd_sum`
(1-instruction simdgroup reductions, ~6-9 cycles each), per-thread
N_READS vectorization (each thread reads 4-8 floats via vector load),
and two-stage threadgroup-shared accumulator + barrier. Our UOp
renderer can't emit those primitives yet — that's the architectural
backlog. Raw MSL has full access.

## Reference: MLX's softmax kernel (read this first)

`/Users/swish/src/thvm/external/mlx/mlx/backend/metal/kernels/softmax.h`

Two variants:
- `softmax_single_row` (lines 11-98): one threadgroup per row,
  each thread reads `N_READS` consecutive floats. Two-stage reduce:
  per-thread find max → `simd_max` → write per-simdgroup partial to
  `local_max[]` → barrier → first simdgroup reduces partials with
  `simd_max` again. Same pattern for `simd_sum`. Hits ~MLX's
  baseline.
- `softmax_looped` (lines 100-190): same shape but with an outer
  loop for axis sizes too large for one threadgroup pass. Uses the
  online-softmax recurrence (running max + scale-prev-norm-by-
  exp(prev_max - new_max)). MLX's preferred large-shape variant.

`SOFTMAX_N_READS` is a compile-time constant; MLX picks 4 or 8
based on dtype/axis_size (see `/Users/swish/src/thvm/external/mlx/mlx/backend/metal/softmax.cpp`).

## Tools

- `from py.thvm import Metal` — `m.compile_msl(src, fn='k') → pso_handle`
- `m.buf_alloc / buf_write_array / buf_read_array / dispatch / dispatch_timed`
- `mx.softmax(x, axis=-1)` — the bar (auto-baselined by score.sh)

## Workspace files

- `kernel.metal` — the file you edit. Contains a working naive softmax
  baseline; replace with your MLX-inspired implementation.
- `score.sh / score.py` — runs your kernel, compares to numpy + mlx, prints
  5 machine-parseable lines + max_abs_err.

## Kernel ABI (do not change)

```msl
[[kernel]] void k(
    device const float *in     [[buffer(0)]],   // [R*C] flat
    device       float *out    [[buffer(1)]],   // [R*C] flat
    constant     int   &axis_size [[buffer(2)]],  // = C
    /* you choose: */ uint gid [[threadgroup_position_in_grid]],
                      uint lid [[thread_position_in_threadgroup]],
                      uint simd_lane_id [[thread_index_in_simdgroup]],
                      uint simd_group_id [[simdgroup_index_in_threadgroup]]);
```

Dispatch: agent decides via the dispatch.json file:
```
{"grid": [R*tg_threads, 1, 1], "threadgroup": [tg_threads, 1, 1]}
```
(score.py reads dispatch.json next to kernel.metal)

## Iteration protocol

1. Edit `kernel.metal` (and optionally `dispatch.json`).
2. Run `./score.sh 32 256` and/or `./score.sh 4096 4096`.
3. Read 5-line output. Iterate.

## Stop conditions

- `speedup_vs_mlx ≥ 1.05×` p50 at BOTH (32, 256) AND (4096, 4096), OR
- 12 iterations completed.

## Hints

- `simd_max(x)` and `simd_sum(x)` are simdgroup-collective; all 32 lanes contribute.
- `fast::exp(x)` is the fast intrinsic — MLX's approach.
- Vectorize loads: `device const float4 *in_v = (device const float4*)in;`
- Each row needs ≥ 32 threads to use simdgroup reduce; `tg_threads = min(C, 1024)`.

## Final report

Write `RESULTS.md`. Include:
- One-line-per-iteration log: `[iter N] change: <X>; result: <Y>`
- Final kernel.metal + dispatch.json
- Final speedup_vs_mlx at BOTH shapes (3 fresh runs each for variance)
- 1 thing you found surprising about MLX's source
- 2-3 micro-optimizations you'd try with more budget
