# MLX reference: what to copy, what's hard to beat

MLX is the production-grade ML framework for Apple Silicon.  Its
Metal kernels are aggressively tuned and represent close to the
practical roofline for FP32 compute on M-series GPUs.  Beating MLX
by even 5% on a common op is meaningful; beating it by >20% almost
always means the workload structure changed (fusion, online algorithm,
better numerical formulation), not just micro-optimization.

This doc points to the exact MLX kernel sources for the four most
common ops, summarises the techniques each uses, and flags the gaps
between MLX and what thvm's renderer can emit today.

## Where to read MLX kernels

```
external/mlx/mlx/backend/metal/kernels/
```

Key files:

| File | Op | Highlights |
|---|---|---|
| `softmax.h` | softmax | `softmax_single_row` + `softmax_looped`; two-stage simdgroup reduce; `-FLT_MAX` sentinel |
| `steel/gemm/`, `steel_gemm.metal` | matmul | `simdgroup_matrix<float, 8, 8>`; cooperative load; multi-frag accumulator; tile picker in `mlx/backend/metal/matmul.cpp` |
| `reduction/reduce_*.h` | reduce | All / col / row variants; `simd_sum`/`simd_max` + threadgroup reduce |
| `layer_norm.metal` | layernorm | Two-pass mean+variance in a single kernel launch (mean first, then centered sum-of-squares using the cached mean) -- NOT Welford |

The C++ shape-dispatcher (e.g. `mlx/backend/metal/softmax.cpp`)
picks which kernel to run based on (R, C, dtype) -- look there to
see MLX's hard-coded shape table for what tile/N_READS/lsize each
shape gets.

## The five techniques to know

These appear in nearly every MLX kernel:

### 1. Simdgroup-collective reduce (`simd_max`, `simd_sum`)

A `simdgroup` is 32 threads (one warp on NVIDIA).  `simd_sum(x)`
reduces `x` across all 32 lanes in ~6-9 cycles -- one instruction,
hardware-implemented butterfly.  Replaces a 5-step manual
`simd_shuffle_xor` butterfly.

```msl
float local = my_value;
float sg_sum = simd_sum(local);   // all 32 lanes get the same answer
```

For values larger than one simdgroup's worth, do two stages:

```msl
threadgroup float partials[32];   // 32 simdgroups max per TG
float sg_sum = simd_sum(local);
if (simd_lane_id == 0) partials[simd_group_id] = sg_sum;
threadgroup_barrier(mem_flags::mem_threadgroup);
if (simd_group_id == 0) {
    float p = (simd_lane_id < num_simdgroups) ? partials[simd_lane_id] : 0.0f;
    float total = simd_sum(p);
    // total is the threadgroup sum
}
```

thvm's `KOP_SIMD_REDUCE` emits the first stage but **not** the
second (extent-> 32 limit).  See backlog item in
`docs/plans/mlx_features_to_port.md`.

### 2. `fast::exp` (and friends)

`fast::exp(x)` is ~10x faster than `exp(x)` at ~2 ULP error.  Same
for `fast::log`, `fast::sin`, `fast::cos`.  For softmax / GELU /
GLU / etc, this is essentially free accuracy you trade for huge
speedups.

```msl
float e = fast::exp(x - x_max);     // not exp()
```

thvm emits this when `OPT_FAST_MATH` is set on `EXP2`/`LOG2`/`SQRT`
nodes; `KOP_FAST_MATH` wraps every wrappable unary in one pass.

### 3. Per-thread `N_READS` vectorization

Each thread reads `N_READS` consecutive values via vector load
(`float4`, `float8` reinterpreted from `float`).  Shrinks total
threads needed by Nx, frees occupancy budget for register
accumulators.

```msl
constexpr int N_READS = 8;
threadgroup const float4 *in_v = (threadgroup const float4*)in;
float4 vals[N_READS / 4];
for (int i = 0; i < N_READS / 4; i++) {
    vals[i] = in_v[lid * (N_READS / 4) + i];
}
```

`SOFTMAX_N_READS` in MLX is `4` (constant, defined in
`external/mlx/mlx/backend/metal/kernels/defines.h:14`).  Sweeping
N_READS ∈ {4, 8, 16, 32} at (R=4096, C=4096) gives all four within
2% of each other -- N_READS tuning is not where the win is.

thvm's `KOP_VEC_LOAD(width)` wraps `INDEX_E` with `OPT_VEC_LOAD(width)`
which the renderer translates to a `float4*`/`float8*` reinterpret.

### 4. `-FLT_MAX` (NOT `-INFINITY`) for OOB padding

When a kernel has more lanes than data (final partial pass of a
reduction), idle lanes contribute a sentinel.  MLX uses
`Limits<AccT>::min` which is `-FLT_MAX` (most-negative-finite),
**not** `-INFINITY`.

Why: with `-INFINITY`, the online softmax step `normalizer *=
exp(prev_max - new_max)` computes `exp(-inf - -inf) = exp(NaN) =
NaN` and pollutes the reduction.  With `-FLT_MAX`, the same step
underflows cleanly to zero.

```msl
float maxval = (oob ? -FLT_MAX : in_val);
maxval = simd_max(maxval);    // safe even if all lanes are OOB
```

Found in `mlx/backend/metal/kernels/softmax.h:47-48`.  This is the
single most subtle invariant in MLX softmax -- the iter-1 NaN trap
that the prior `agent_softmax_msl` run hit (see its `RESULTS.md`).

### 5. `simdgroup_matrix<float, 8, 8>` MMA + multi-frag accumulator

For matmul, MLX uses cooperative loads into threadgroup memory plus
`simdgroup_matrix` for the inner MMA.  Each simdgroup holds an 8x8
fp32 frag and chains multiple frags per accumulator tile (BM, BN
larger than 8).

```msl
#include <metal_simdgroup_matrix>
simdgroup_matrix<float, 8, 8> Aregs[BM/8];
simdgroup_matrix<float, 8, 8> Bregs[BN/8];
simdgroup_matrix<float, 8, 8> Cregs[BM/8 * BN/8];
// load Aregs from threadgroup-shared via simdgroup_load
// chain BM/8 * BN/8 simdgroup_multiply_accumulate calls
// store Cregs to global via simdgroup_store
```

Throughput per MMA: ~17-18 cycles on M3.  See
`philipturner/metal-benchmarks` for instruction-level numbers.

thvm's `KOP_TC(factor)` wraps the inner REDUCE with `OPT_TC` which
the renderer translates to `simdgroup_matrix` scaffolding.  Tile
sizes 8/16/32 supported; multi-frag accumulator (BM/BN > 8) is
backlog (see `docs/plans/mlx_features_to_port.md` feature 5).

## What MLX does that thvm cannot emit yet

From `docs/plans/mlx_features_to_port.md`:

| Feature | Status | Workaround |
|---|---|---|
| Two-stage TG reduce | Ported only stage 1 (`KOP_SIMD_REDUCE`) | Write raw MSL for extents > 32 lanes |
| Online softmax | Not ported | Write raw MSL |
| Multi-frag MMA accumulator | `KOP_TC` is single-frag | Write raw MSL with chained `simdgroup_matrix` |
| Software-pipelined matmul | Not ported | Write raw MSL with two threadgroup-shared staging buffers |
| Two-pass layernorm in one kernel launch | Not ported | Write raw MSL (or use `mx.fast.layer_norm` baseline directly) |
| Kernel fusion (softmax+matmul, etc) | Not ported | Write raw MSL |

For these, the raw-MSL track (see [msl_writing.md](msl_writing.md))
is your only path.  Drop down to MSL, beat MLX, then optionally port
the technique back into thvm's renderer as a new `OPT_*` annotation.

## What MLX leaves on the table

From the agent runs in May 2026 -- techniques that beat MLX cleanly:

- **Multi-row-per-TG (softmax, large C)**: pack N rows into one
  threadgroup with per-row slices of `local_max[]` /
  `local_normalizer[]`.  Halves dispatched TG count -> cuts per-TG
  encoder + barrier overhead.  MLX's kernel is templated on a fixed
  N_READS and would need a structural rewrite for this.  +5-9% at
  (4096, 4096) softmax.
- **No-TG-memory path (softmax / reduce, small C)**: 1 simdgroup
  per row with strided loads + `simd_max`/`simd_sum` only -- no
  `local_*[]`, no `threadgroup_barrier`.  Pack 4 SGs per TG to
  spread work across the GPU's 40 cores.  +5-9% at (32, 256) softmax.
- **Cooperative single-dispatch reduce (vector_sum)**: avoid MLX's
  ~100us/dispatch CPU overhead from running two separate kernels.
  Last-TG-wins via atomic counter; one TG runs the second-stage
  reduce in the same dispatch.  +5-7% at N=16M sum.
- **Cached `1/axis_size` (layernorm)**: compute once, multiply
  rather than divide.  Saves 2 FP divides per row.  Modest but free
  in any per-row kernel.
- **Shape-adaptive `lsize` per kernel call**: layernorm at C=768
  wants lsize=96, at C=2048 wants lsize=256.  MLX's dispatcher does
  this; if you write one kernel for all shapes you must too.

## Reference: 6 MLX features and where they live

From `docs/plans/mlx_features_to_port.md`:

1. **`simd_max`/`simd_sum`** -- `external/mlx/mlx/backend/metal/kernels/softmax.h:50-60`
2. **Two-stage TG reduce** -- `external/mlx/mlx/backend/metal/kernels/softmax.h:60-90`
3. **`fast::exp`** -- `external/mlx/mlx/backend/metal/kernels/softmax.h:80`
4. **N_READS vectorization** -- `external/mlx/mlx/backend/metal/kernels/softmax.h:30-45`
5. **`float4` reinterpret** -- `external/mlx/mlx/backend/metal/kernels/softmax.h:35`
6. **`-FLT_MAX` (finite) sentinel** -- `external/mlx/mlx/backend/metal/kernels/softmax.h:47-48`

## Apple GPU microarchitecture numbers

From `philipturner/metal-benchmarks` -- ground truth for any
roofline analysis:

| Instruction / Op | Throughput |
|---|---|
| `simdgroup_matrix<float, 8, 8>` MMA | ~17-18 cycles |
| `simd_shuffle` / `simd_broadcast` | ~2 cycles |
| `simd_sum` / `simd_max` | ~6-9 cycles |
| Per-core memory bandwidth | ~32 B / GPU data cycle |
| System cache bandwidth | ~15-20 B / cycle |
| Threadgroup memory | ~60 KB per core (M1/M2; M3+ similar) |

| Hardware | Spec |
|---|---|
| simdgroup width | 32 threads |
| `maxTotalThreadsPerThreadgroup` | typically 1024 |
| `maxThreadgroupMemoryLength` | 32 KB on M1, ~64 KB on M3 Max |
| Unified memory | shared with CPU; `MTLResourceStorageModeShared` is free |
| Tensor cores (NVIDIA-style) | none; closest is `simdgroup_matrix` |

For an M3 Max specifically (40 GPU cores):

- FP32 peak: ~14.2 TFLOPS
- Memory bandwidth: ~400 GB/s
- MLX matmul reaches 66% of FP32 peak at L=2048 (see
  `bench/metal-problems/matmul/RESULTS.md`)
- MLX softmax tied or +5-30% over a hand-tuned single_row variant
  in prior agent runs
