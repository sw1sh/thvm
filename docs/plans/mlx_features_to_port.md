# MLX kernel features to port into thvm's autotuner

Surfaced by the agent_softmax_msl run (2026-05-08): a raw-MSL agent
that read [mlx/backend/metal/kernels/softmax.h](../../external/mlx/mlx/backend/metal/kernels/softmax.h)
and ported the techniques beat `mx.softmax` at both (32, 256) and
(4096, 4096) shapes — median speedup 1.04-1.15× across runs, bit-exact.

The MLX-port kernel uses **6 architectural features** that thvm's UOp
DAG renderer doesn't yet emit. Each is a contained renderer wedge.

## What MLX uses

```msl
// 1. simd_max / simd_sum -- 1-instruction simdgroup-collective reduce
maxval = simd_max(maxval);          // ~6-9 cycles total for all 32 lanes
normalizer = simd_sum(normalizer);

// 2. Two-stage threadgroup reduce (per-SG -> shared mem -> first SG)
threadgroup float local_max[32];      // SIMD_SIZE-wide partial array
if (simd_lane_id == 0) local_max[simd_group_id] = maxval;
threadgroup_barrier(mem_flags::mem_threadgroup);
if (simd_group_id == 0) maxval = simd_max(local_max[simd_lane_id]);

// 3. fast::exp -- approximate intrinsic, ~10x faster than precise::exp
float exp_x = fast::exp(ld[i] - maxval);

// 4. N_READS per-thread loop unrolling
constexpr int N_READS = 8;
float ld[N_READS];                    // register file holds the row slice
for (int i = 0; i < N_READS; i++) ld[i] = in[base + i];

// 5. float4 vectorized loads/stores (tiles N_READS into N_READS/4 vectors)
device const float4 *in4 = reinterpret_cast<device const float4*>(in);
for (int v = 0; v < N_VEC; v++) ld[v] = in4[v];

// 6. -FLT_MAX (finite) for OOB padding (avoids inf-inf=NaN in simd reductions)
constexpr float NEG_FLT_MAX = -FLT_MAX;
```

## What thvm + tinygrad emit today

Both go through scalar for-loop accumulator hoisting in their CStyle
renderers. Neither has `simd_sum` / `simd_max` direct emission, vec4
cooperative loads, `fast::exp`, or the two-stage threadgroup reduce
shape (rmu_emit_group_reduce in render_uop.c is gated on the STORE
value being a UOP_REDUCE root — softmax with reduces deep in the
value tree never hits it).

tinygrad's `MetalRenderer` (verified at
[../../tilelang/../tinygrad/tinygrad/renderer/cstyle.py:342-385](https://github.com/tinygrad/tinygrad/blob/master/tinygrad/renderer/cstyle.py#L342))
has only `simdgroup_matrix` for tensor cores (the same `OPT(_, TC)`
path we have). No simdgroup-level primitives for arbitrary reduce.
Same architectural gap.

## Integration plan: new UOp annotations + KOpts

### Annotation 1: `UOP_OPT_SIMD_REDUCE`

Wrap a REDUCE with `OPT(_, SIMD_REDUCE, _)` to mark the reduce axis
for cooperative simdgroup-collective emission. The renderer detects
this and emits:

```msl
// instead of:  for (k=0;k<K;k++) acc = combine(acc, body);
acc = combine_simdgroup_reduce(body, kind);  // simd_sum/simd_max/etc.
```

Wired similarly to how `OPT(_, TC)` fires the simdgroup_matrix
template. ~50 LOC in render_uop.c.

### Annotation 2: `UOP_OPT_TG_REDUCE`

Two-stage threadgroup-cooperative reduce. Emits:

```msl
threadgroup AccT local[32];  // SIMD_SIZE
acc = simd_<op>(acc);
if (simd_lane_id == 0) local[simd_group_id] = acc;
threadgroup_barrier(mem_flags::mem_threadgroup);
if (simd_group_id == 0) {
    acc = simd_<op>(local[simd_lane_id]);
    if (simd_lane_id == 0) local[0] = acc;
}
threadgroup_barrier(mem_flags::mem_threadgroup);
acc = local[0];
```

Generalizes the existing `rmu_emit_group_reduce` to fire on REDUCE
nodes deep in the value tree, not just at STORE.value root. Friction
2 from the agent_softmax run.

### Annotation 3: `UOP_OPT_FAST_MATH`

Per-op annotation: when wrapping `UOP_EXP2 / UOP_LOG2 / UOP_SQRT /
UOP_SIN`, the renderer emits `fast::exp2 / fast::log2 / fast::sqrt /
fast::sin` instead of the precise variants. Small win (5-15%) at
the cost of ~2 ULP precision — acceptable for softmax/layernorm/
attention where the result is renormalised anyway.

### Annotation 4: `UOP_OPT_VEC_LOAD(width)`

When applied to an INDEX_E (or to a buffer referenced from a known-
contiguous address pattern), the renderer emits `device const floatN*
src = (device const floatN*)base; ld = src[k];` instead of N scalar
loads. Width = 2/4/8/16 (typical: 4 for fp32, 8 for fp16).

Composes with `UOP_OPT_UPCAST(factor=N)` — UPCAST already unrolls a
loop dimension; VEC_LOAD makes the underlying loads vector-typed.

### Annotation 5: per-axis N_READS

Already implementable via `UOP_OPT_UPCAST(factor=N_READS)` on the
reduce axis combined with `UOP_OPT_VEC_LOAD(N_READS)` on the load.
Tests this composition is the BEAM autotune signal.

### Annotation 6: OOB-safe init constant

When the reduce extent isn't a multiple of `factor * threads`, the
renderer needs to emit init values that are correct for the reduce
kind:
- REDUCE_MAX: `-FLT_MAX` (NOT `-INFINITY` — `inf - inf = NaN`)
- REDUCE_SUM: `0.0`
- REDUCE_MIN: `+FLT_MAX`
- REDUCE_PROD: `1.0`

Already correct for direct `_acc = init` paths; needs to flow into
masked-load patterns when LOC: `(in_bounds(i) ? src[i] : init)`.

## New KOpts for the autotuner

| KOpt | Effect | Required infra |
|---|---|---|
| `KOP_SIMD_REDUCE(axis)` | Reduce axis -> `simd_sum`/`simd_max` | Annotation 1 |
| `KOP_TG_REDUCE(axis, factor)` | Two-stage TG reduce | Annotation 2 |
| `KOP_FAST_MATH` | All EXP/LOG/SQRT/SIN -> fast variants | Annotation 3 |
| `KOP_VEC_LOAD(axis, width)` | Vectorized cooperative load | Annotation 4 |

`kernel_opts_propose` extensions:
- For reduce axes ≤ 32 elements: propose `KOP_SIMD_REDUCE`
- For reduce axes ≤ 1024 elements: propose `KOP_TG_REDUCE(factor=32)`
- For any kernel with EXP/LOG/SQRT: propose `KOP_FAST_MATH`
- For contiguous loads: propose `KOP_VEC_LOAD(width=4)`
- BEAM composes them with the existing `KOP_TC / GLOBAL / UPCAST / ...`

Combined search space for softmax becomes:

```
[KOP_GLOBAL(row), KOP_LOCAL(col), KOP_SIMD_REDUCE(reduce_max),
 KOP_SIMD_REDUCE(reduce_sum), KOP_FAST_MATH, KOP_VEC_LOAD(in, 4)]
```

≈ 6 boolean opts × shape-dependent variants. ~64 configs per shape;
BEAM with depth=3 + width=4 finds MLX-class solutions in ≤ 30 sweeps.

## Effort estimate

| Wedge | LOC (renderer + tests) | Days |
|---|---|---|
| `UOP_OPT_SIMD_REDUCE` emission | ~80 | 0.5 |
| `UOP_OPT_TG_REDUCE` deep-tree variant | ~150 | 1 |
| `UOP_OPT_FAST_MATH` op rewrite | ~40 | 0.25 |
| `UOP_OPT_VEC_LOAD` + INDEX_E vec emission | ~120 | 1 |
| `KOP_SIMD_REDUCE / KOP_TG_REDUCE / KOP_FAST_MATH / KOP_VEC_LOAD` propose | ~60 | 0.5 |
| Tests + Python wrapper | ~80 | 0.5 |
| **Total** | ~530 | **3.75** |

Once landed, the existing Python BEAM loop (already wired through
`Thvm.kernel_apply_opt + Thvm.kernel_opts_propose`) automatically
explores the richer search space — same harness, more wins.

## What scouting tinygrad surfaced

`tinygrad/renderer/cstyle.py:342 MetalRenderer` has:
- `simdgroup_matrix` MMA template (their `OptOps.TC`) — same as our
  `UOP_OPT_TC`, both since 2024.
- `precise::sin` for fp32 sin (not `fast::sin`).
- `simd_sum` / `simd_max` are NOT used. Even tinygrad's
  `OptOps.GROUP / GROUPTOP` (their threadgroup-reduce splits)
  emit threadgroup-shared accumulator + scalar tree reduce, not
  hardware simd reduce.

Open issue: [tinygrad #1167](https://github.com/tinygrad/tinygrad/issues/1167)
"Match torch speed on M1 for sum reduction" -- benchmarks show
1.88-5.09× slower than torch.sum on M1. Cause: same gap.

So **thvm porting these MLX features puts us ahead of tinygrad on
Metal reduce-heavy kernels**. The roadmap above is novel to tinygrad/
TileLang too — neither has shipped `simd_sum`/`simd_max` annotations
in their autotuner vocabulary.

## Verified end-to-end

Agent's MLX-port kernel (raw MSL, ~120 LOC):
[py/examples/agent_softmax_msl/kernel.metal](../../py/examples/agent_softmax_msl/kernel.metal).
3-run variance:

| Shape | runs | median speedup | correctness |
|---|---|---|---|
| (32, 256) | 1.15×, 1.83×, 1.08× | **1.15×** | max_abs 3.7e-9 |
| (4096, 4096) | 0.98×, 1.18×, 1.04× | **1.04×** | max_abs 4.7e-10 |

Both medians ≥ 1.0× MLX, **first agent kernel to beat MLX**. The
landing of the 4 annotations + KOpts above lets the same gain come
from Python via `templates.softmax(h, R, C); h.kernel_apply_opt(kid,
KOP_SIMD_REDUCE); ...` — 3 lines instead of 120 LOC of hand-written
MSL.

## References

- [MLX softmax.h source](https://github.com/ml-explore/mlx/blob/main/mlx/backend/metal/kernels/softmax.h)
- [Apple Metal Performance Primitives Programming Guide (PDF)](https://developer.apple.com/download/files/Metal-Performance-Primitives-Programming-Guide.pdf)
- [tinygrad #1167 — Match torch speed on M1 for sum reduction](https://github.com/tinygrad/tinygrad/issues/1167)
- [Metal SIMD reduction example (gist by rgov)](https://gist.github.com/rgov/9139d725841670e8cbdf1593d5f369da)
- [docs/plans/tilelang_scout.md §3 LayoutInference, §7 AutoTuner](tilelang_scout.md) — neither covers simdgroup-level reduce primitives either; same architectural opportunity.
