# vector_add via raw MSL -- agent task: MATCH/BEAT MLX

Goal: write a raw Metal Shading Language fp32 elementwise add kernel
(`c = a + b`) that beats MLX (`a + b`) on Apple M-series GPU at:

- N = 1_048_576   (1M floats; dispatch-overhead visible)
- N = 16_777_216  (16M floats; pure bandwidth)

Read `docs/auto_metal_kernels/agent_brief.md` first.

## Honest expectation

vector_add is **memory-bandwidth bound**: 3 buffer touches (read a,
read b, write c) and one add.  There is almost no compute to
optimize -- the kernel is one `add` away from peak bandwidth even
naive.  MLX is already at the bandwidth roofline.

The realistic target: **match MLX within noise** at N=16M, and beat
it slightly at N=1M by **cutting dispatch overhead** (fewer, fatter
threadgroups; `float4` vectorized loads so each thread does 4-16
elements via a grid-stride loop).  A clean 1.05x is plausible at the
small size where MLX pays per-dispatch overhead; at 16M expect ~1.0x.

This problem is in the corpus for completeness and as a bandwidth
sanity check -- treat ~1.0x at 16M as success, not failure.

## ABI (do not change)

```msl
[[kernel]] void k(
    device       float *out [[buffer(0)]],   // [N]
    device const float *a   [[buffer(1)]],   // [N]
    device const float *b   [[buffer(2)]],   // [N]
    /* you choose the builtins */);
```

`N` arrives as compile-time `#define K_N`.

## What to try

1. `float4` reinterpret: `(device const float4*)a` -- 1 vector load
   instead of 4 scalar loads.  Handle the `N % 4` tail.
2. Grid-stride loop: each thread does `K` elements (`K` = 4, 8, 16),
   `grid = ceildiv(N, K)` threads.  Fewer threadgroups -> less
   encoder overhead.
3. Sweep `threadgroup` size (256 / 512 / 1024) and `K`.

## dispatch.json

`rule` -> threadgroup total threads, `grid_rule` -> grid total
threads.  Exprs eval'd with `N ceildiv max min simd tg` in scope.
For float4 + each thread doing 4 elements:

```json
{"rule": "256", "grid_rule": "ceildiv(N, 4*256) * 256"}
```

## Iteration loop

1. Edit `kernel.metal` (one change per iteration).
2. `./score.sh 1048576` and `./score.sh 16777216`.
3. Read the score block, append a row to `RESULTS.md`.

## Score output

8 lines.  `speedup_gpu` is the headline (GPU-time vs GPU-time);
`speedup_wall` is the legacy wall metric.  For bandwidth-bound
kernels `speedup_gpu` is the honest number.

## Stop conditions

- `speedup_gpu >= 1.05x` median (3 fresh runs) at N=1M AND
  `speedup_gpu >= 0.97x` at N=16M, OR
- 10 iterations.

## Final report

Write `RESULTS.md`: iteration log, final kernel + dispatch, 3-run
variance, achieved GB/s + % of memory-bandwidth roofline (M3 Max
~400 GB/s) at each size, one surprising finding, follow-ups.
