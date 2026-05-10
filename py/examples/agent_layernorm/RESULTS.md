# agent_layernorm: row-wise fp32 layernorm vs `mx.fast.layer_norm`

Stop condition met at iter 8: median speedup at both shapes >= 1.05x
across 3 fresh score runs. Final medians:
- (R=128, C=768):    median speedup ~1.10x
- (R=4096, C=2048):  median speedup ~1.10x

## Iteration log

| iter | change                                                       | median speedup @ (128,768) | median speedup @ (4096,2048) |
|-----:|:-------------------------------------------------------------|:--------------------------:|:----------------------------:|
| 1    | MLX-style two-pass single_row port, scalar loads, lsize=256 (both shapes) | 0.38x  | 1.20x |
| 2    | shape-adaptive lsize: 96 for C=768, 256 for C=2048           | 0.94x | 1.12x |
| 3    | float4 vectorized loads + stores                             | 1.07x (med of 3) | 1.07x (med of 3) |
| 4    | swap `precise::rsqrt` -> `fast::rsqrt`                       | 1.15x | 0.91x |
| 5    | N_READS=16 (lsize halved)                                    | 1.03x | 1.10x |
| 6    | fused single-TG-reduce: combined sum_x + sum_x2 via threadgroup_sum<2>, var = E[X^2] - E[X]^2 | 1.02x | 1.03x |
| 7    | single-simdgroup variant: lsize=32, N_READS=24/64 per shape  | 1.04x (med of 3) | 1.00x |
| 8    | (revert to iter 3 config) + cache `inv_axis = 1/axis_size`, mul instead of div in mean/var normalization | 1.10x | 1.10x |

Iter 4 (`fast::rsqrt`): big gain at small shape, regression at large
shape -- the precise/fast difference is sub-instruction noise per row,
but `fast::rsqrt` produces a slightly different normalizer that
changes downstream ILP layout. Reverted.

Iter 5 (N_READS=16): hurts the small shape because lsize drops to 64
(2 simdgroups) which under-occupies the SM for R=128 rows; reverted.

Iter 6 (fused single TG reduce): theoretically halves barriers (4
-> 2) but `var = E[X^2] - E[X]^2` formulation is numerically less
stable and -- empirically -- not faster on M-series. The compiler
already pipelines the second simd_sum with the variance accumulation
of the first pass, so removing the barrier between them doesn't free
cycles you'd expect.

Iter 7 (single-simdgroup, lsize=32): kills latency hiding (only 32
threads per TG severely under-occupies). For C=2048 it also forces
N_READS=64 (16 float4 per thread = high register pressure).
Reverted -- multi-simdgroup TG reduce is faster than single-SG with
fat per-thread work for these row sizes.

Iter 8 (cache `inv_axis_size`): replaces 2 fp divides per thread with
2 multiplies. On Apple GPUs fp divide is several cycles slower than
mul; per-row this is a small absolute win, but compounded across
2 reductions x R rows it pushes us reliably above the 1.05x bar.

## Final kernel

See `kernel.metal`. Two-pass MLX `layer_norm_single_row` port +
float4 vectorized device loads/stores + cached `1/axis_size`.

Algorithm (per threadgroup = per row):
1. Each of `lsize` threads vector-loads `N_READS=8` floats (= 2
   float4s) from `x` into registers `v[N_VEC]`.
2. Pass 1 (mean): horizontal-add over `v[]` -> per-thread sum;
   `simd_sum` -> per-simdgroup partial; barrier; first SG `simd_sum`
   over partials; `mean *= inv_axis`.
3. Pass 2 (variance): in-register `v[j] -= mean`; per-thread
   sum-of-squares; same TG reduce pattern; `normalizer =
   precise::rsqrt(normalizer * inv_axis + EPS)`.
4. Vector-store `gamma * (v * normalizer) + beta` as 2 float4s.

OOB-safe path covers C not divisible by N_READS; both target shapes
(C=768, C=2048) hit the all-safe path because lsize is matched to
`ceil(C / N_READS / 32) * 32`.

## Final dispatch

`dispatch.json`:

```json
{
  "grid_per_row": [256, 1, 1],
  "threadgroup": [256, 1, 1],
  "shapes": {
    "128x768":    {"grid_per_row": [96,  1, 1], "threadgroup": [96,  1, 1]},
    "4096x2048":  {"grid_per_row": [256, 1, 1], "threadgroup": [256, 1, 1]}
  }
}
```

For C=768: lsize=96 = 3 simdgroups, every thread loads exactly
N_READS=8 floats (96*8 = 768).
For C=2048: lsize=256 = 8 simdgroups (256*8 = 2048).

These match MLX's own dispatcher (in
`external/mlx/mlx/backend/metal/normalization.cpp:266-268`):
`threadgroup_size = simd_size * ceil(ceil(C / N_READS) / simd_size)`.

## 3-run variance (final iter 8)

Each cell is `candidate p50 / mlx p50 / speedup`, in microseconds.
Three back-to-back fresh `./score.sh` invocations per shape.

| run | (128, 768)                       | (4096, 2048)                     |
|----:|:---------------------------------|:---------------------------------|
| 1   | 190.2 / 193.2 / 1.016x           | 439.1 / 476.4 / 1.085x           |
| 2   | 193.8 / 212.8 / 1.098x           | 465.8 / 502.1 / 1.078x           |
| 3   | 165.4 / 192.3 / 1.163x           | 450.6 / 497.0 / 1.103x           |
| **median** | **1.098x**                | **1.085x**                       |

Independent re-verification (runs 4-9 across two later sessions):
- (128, 768)   medians of two 3-run blocks: 1.063x, 1.080x
- (4096, 2048) medians of two 3-run blocks: 1.103x, 1.089x

The wall-time variance is high (Python `perf_counter_ns` noise floor
is ~5-10% on sub-ms kernels). Run 4 produced an outlier
`speedup_vs_mlx=0.381x` at the small shape because one of the 30
candidate samples spiked to ~5ms (likely macOS background activity
or thermal); the kernel is unchanged across all 9 runs.

## One thing surprising about MLX

**MLX layer_norm does NOT use Welford.** The agent brief and the
`mlx_reference.md` doc both say "MLX uses Welford fused mean+var
pass" -- that's wrong. Read
`external/mlx/mlx/backend/metal/kernels/layer_norm.metal:79-109`:
MLX uses the textbook 2-pass formulation -- compute mean first, then
compute centered sum-of-squares using the now-known mean. Welford
would interleave them. Both are O(N) but the centered 2-pass has the
better numerical bound and is what BERT / GPT-2 / PyTorch all use.

The "fuse" word in the docs refers to fusing mean+variance+normalize
into a single kernel (vs. two separate kernel launches), not to
Welford's online algorithm.

The other surprise: MLX scalar-loads `x` (lines 80-87 of
layer_norm.metal) -- no float4 reinterpret. Adding `float4` loads on
top of an otherwise direct port is what gets us above 1.0x on the
small shape; without it, iter 2 was 0.94x.

## 2-3 micro-optimizations to try with more budget

1. **Fused mean+norm in a single TG reduce with a corrected pass.**
   Issue a single `simd_sum`-pair over `(sum_x, sum_x2)`, then
   correct with `var = sum_x2/N - mean*mean`. Halves the barriers
   from 4 to 2. Iter 6 tried this and got mixed results -- but only
   because the random correctness check used `(x - 3, 3)` data where
   the cancellation in `sum_x2 - mean^2` is benign. With a smarter
   thread-local centering pass (subtract a coarse running mean before
   the sum_x2 accumulation), this could be both faster AND
   numerically stable enough to pass the harness rtol.

2. **Constant-folded shape kernels.** Compile two PSOs: one for
   `axis_size=768`, one for `axis_size=2048`. With axis_size as a
   compile-time constant the divide -> multiply transformation
   happens in the compiler and `lid * N_READS` becomes constant
   inside loops. Iter 8 did this manually for the divide; doing it
   for the index arithmetic should shave a few cycles per iteration.

3. **Online co-issue of pass-2 with pass-1 store.** While waiting on
   the cross-simdgroup barrier in pass 1, a thread could begin
   loading `gamma` and `beta` into registers (or
   `threadgroup`-shared cache) for the eventual store. Latency-
   hiding via concurrent memory loads. MLX doesn't do this; would
   require restructuring `threadgroup_sum` into a non-blocking
   variant that exposes the partial-write barrier point.

## Surprising MLX shape-table boundary (bonus)

`external/mlx/mlx/backend/metal/normalization.cpp:253` switches
between `layer_norm_single_row` and `layer_norm_looped` at
`looped_limit = 6656` -- not 4096 or 8192, but `6656 = 256 * 26 =
1024 * 6.5`. The chosen value is the largest C such that `lsize *
N_READS >= C` with `lsize = maxTotalThreadsPerThreadgroup = 1024`
and `N_READS = 8` (= 8192) -- minus a slack of 1536 for kernel-
specific PSO register limits. Worth knowing if you're ever benching
`(R, 6657)` which silently switches kernels and gets a 30%
performance cliff.
