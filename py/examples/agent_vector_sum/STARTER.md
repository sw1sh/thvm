# vector_sum via raw MSL — agent task: BEAT MLX

Goal: write a raw Metal Shading Language fp32 vector_sum kernel
(`s = sum(a)`) that beats `mx.sum(a)` at `N=65_536` (small dispatch
floor) and `N=16_777_216` (16M floats, memory-bandwidth dominated)
on Apple M-series GPU.

## Reference: MLX's `all_reduce` kernel

`/Users/swish/src/thvm/external/mlx/mlx/backend/metal/kernels/reduction/reduce_all.h`

Two regimes (dispatcher in `mlx/backend/metal/reduce.cpp`):
- `in_size <= REDUCE_N_READS * 1024` (== 4096 floats): single TG,
  `threadgroup_size = ceil(in_size / N_READS)` rounded up to multiple
  of 32. One pass.
- larger: two-pass.
  - Pass 1: `n_rows = 32 * REDUCE_N_READS = 128` TGs (or 1024*4 for
    arrays >= 64MB), each computing one partial.
  - Pass 2: 32-thread TG sums the n_rows partials with `simd_sum`.

`REDUCE_N_READS = 4` (defined in `mlx/backend/metal/kernels/defines.h`).

## Workspace files

- `kernel.metal` — single MSL kernel `k`. Cooperative two-stage pattern:
  every TG computes a partial sum and writes to `part[tg_id]`. After a
  device-wide barrier, each TG atomically increments a counter. The TG
  whose increment lands on `num_tgs - 1` is the "last" — it loads all
  partials and reduces with `simd_sum`, writing to `out[0]`.
  Avoids a second CPU-side dispatch (its ~100us scheduling overhead),
  and avoids the contention of N_TGs all atomic-CASing into `out[0]`.
- `dispatch.json` — `lsize`, `n_reads`, `num_tgs_min_work` (work per
  TG floor), `num_tgs_cap` (max TGs across the dispatch). The Python
  harness picks `num_tgs = max(1, min(cap, ceil(N/min_work)))`.
- `score.py` / `score.sh` — runs the kernel, compares to high-precision
  numpy sum and `mx.sum`, prints the 5-line scoreboard.

## Kernel ABI

```msl
[[kernel]] void k(
    device const float *in       [[buffer(0)]],   // [N]
    device       float *out      [[buffer(1)]],   // [1]
    constant     int   &n        [[buffer(2)]],
    device       float *part     [[buffer(3)]],   // [num_tgs] scratch
    device atomic_uint *counter  [[buffer(4)]],   // [1] init 0; resets self
    /* builtins for tg_id, num_tgs, lid, lsize, simd_lane_id,
       simd_group_id, simds_per_tg */ );
```

Dispatch: `grid = (lsize * num_tgs, 1, 1)`, `threadgroup = (lsize, 1, 1)`.

## Iteration protocol

1. Edit `kernel.metal` (and optionally `dispatch.json`).
2. Run `./score.sh 65536` and `./score.sh 16777216`.
3. Read 5-line output. Iterate.

## Stop conditions

- median speedup_vs_mlx ≥ 1.05x at BOTH sizes across 3 fresh score
  runs each, OR
- 12 iterations completed.

## Hints

- Apple GPU `simd_sum(x)` reduces 32 lanes in ~6-9 cycles (one
  hardware butterfly).
- `float4` reinterpret on `device const float*` lets one issue
  cover 16 bytes -- expose memory-level parallelism.
- Atomic add on fp32 needs `atomic_compare_exchange_weak_explicit`
  via bit-cast through `uint`. Slow if many lanes contend; bound the
  contention by limiting num_tgs.
- `memory_order_acq_rel` is NOT supported by MSL atomics — only
  `memory_order_relaxed`. Pair atomics with explicit barriers.
- Threadgroup barriers must be reached by ALL lanes of the TG; placing
  a `threadgroup_barrier` inside `if (lid == 0) { ... }` is undefined
  behavior and will silently corrupt your output.
