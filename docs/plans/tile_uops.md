# Tile UOps Plan

`TileUop` is the scheduling layer above `ScalarUop`.

The scalar graph remains the semantic reference: explicit ranges,
index expressions, loads, stores, ALU, movement wrappers, and reduces.
The tile graph wraps a scalar body with schedule and memory decisions
that future renderers can lower differently for CPU and Metal.

## Current Scaffold

`KernelEntry` now owns an optional `tile_uops` arena:

- slot 0 is `TILE_NONE`;
- live nodes are stored in `[1, n_tile_uops)`;
- `tile_root` stores the root node id, currently either a
  `TILE_LOOP_NEST` scalar plan or a recognized `TILE_MMA` matmul plan;
- `tile_free` is called by `kernel_free_arrays`;
- `tile_validate` checks the root/store/body/axis structure for scalar
  plans and the M/N/K axis/layout contract for `TILE_MMA` plans before
  a renderer or autotuner consumes the plan;
- `tile_collect_plan_info` extracts the validated root into a compact
  `TilePlanInfo` view: tile root/store/reduce/body ids, scalar
  store/index/value/body/reduce ids, dtype, per-axis ids/types/extents,
  and optional `TileGemmInfo` metadata for `TILE_MMA`;
- WL exposes `TKernelTileUops[kid]` for the raw tile arena and
  `TKernelTilePlan[kid]` for the validated `TilePlanInfo` view;
- `tile_loop_axis_count`, `tile_loop_axis_type`, and
  `tile_loop_axis_extent` expose root-loop axis metadata without
  assuming the root is the last emitted node; internally they only
  report data for a valid collected plan;
- tile plans remember the `KernelAxes.version` they were built
  against, and `tile_sync_from_scalar` rebuilds stale plans after
  `TKernelApplyOpt`, autotune resets, or lazy WL introspection;
- rangeify runs a structural scalar-UOp CSE pass before schedule-key
  sharing and tile planning, deduplicating repeated constants,
  address expressions, loads, casts, and ALU nodes while preserving
  range/store/bufferize identity;
- rangeified scalar reductions append their `S_REDUCE_*` range to
  `KernelAxes` even when the original KProg reduction is not the tail
  op, which makes the existing Metal `GROUP_REDUCE` renderer reachable
  through normal `GROUP` autotune candidates; generated Metal tile
  source substitutes the threadgroup accumulator into post-reduce
  scalar expressions after a `GROUP` opt, while the no-opt baseline
  can still fall back to the old Metal route for autotune comparison;
- `TKernelAutotune` keeps the in-process per-program-shape behavior
  and now also persists winning opts under
  `$XDG_CACHE_HOME/thvm/autotune` / `$HOME/.cache/thvm/autotune`
  (override with `THVM_AUTOTUNE_CACHE_DIR`, disable with
  `THVM_AUTOTUNE_CACHE=0` or `THVM_AUTOTUNE_DISABLE_CACHE=1`), so
  repeated Metal tile experiments can skip benchmark rediscovery;
- CPU dispatch has an opt-in `THVM_TILE=1` path that consumes the
  validated tile plan over scalar UOps, records dispatch kind
  `"tile"`, and falls back to the normal BLAS/JIT/scalar paths when
  no supported tile plan is present;
- that tile path first tries a generated C tile renderer for f32/f64
  scalar graphs with `LOOP`/`UPCAST`/`LOCAL`/`GLOBAL` output axes,
  including `S_INDEX_E` addresses built from `S_I*` expression nodes,
  f32/f64 casts, movement wrappers, and scalar `S_REDUCE_SUM` /
  `S_REDUCE_MAX` accumulators;
- tile C accepts `REDUCE`/`UNROLL`/`GROUP_REDUCE` axes only as
  reduction-schedule metadata on scalar graphs that contain a reducer;
  those axes do not become outer output loops yet;
- `tile_analyze_gemm` recognizes matmul-shaped `MUL + REDUCE_SUM`
  programs and recovers a compact `TileGemmInfo` view (`M/N/K`, input
  slots, leading dimensions, transpose flags).  CPU BLAS GEMM consumes
  the shared analysis directly, while Metal direct GEMM consumes the
  validated `TILE_MMA` plan produced from it, so GEMM recognition is
  no longer duplicated inside backend dispatch code.  Metal's current
  `metal-gemm` path lowers that plan to a threadgroup-memory tiled f32
  shader.  `TC` opts select 32/16/8 fixed tile-size variants for
  autotune; generated tile renderers still decline that root and fall
  back to BLAS/direct Metal/scalar execution until full MMA renderers
  land;
- Metal dispatch has a matching opt-in `THVM_TILE=1` path for f32
  `LOCAL`/`GLOBAL` elementwise tile plans.  It compiles
  `cg_emit_tile_metal`, maps `GLOBAL` to
  `threadgroup_position_in_grid`, maps `LOCAL` to
  `thread_position_in_threadgroup`, dispatches
  `GLOBAL x LOCAL` as threadgroups x threads-per-threadgroup, and
  records dispatch kind `"metal-tile"`;
- Metal also has `THVM_METAL_SPECIALIZED=1` diagnostic direct
  conv2d/GEMV paths.  These are not the intended architecture and
  stay off by default; they exist only as correctness/performance
  oracles while the tile planner learns to rediscover equivalent
  schedules from lowered scalar/tile primitives;
- rank-1 `TMatVec` now reaches that generic path: the tile analyzer
  recognizes `EXPAND(vector) -> MUL(matrix, vector) -> REDUCE_SUM`
  as a `TILE_MMA` plan with `N=1`, so Metal dispatches it via
  `"metal-gemm"` and `TKernelProposed` exposes the normal `TC`
  tile-size candidates without using the diagnostic GEMV recognizer;
- generated Metal TileUop source now covers f32 scalar `TILE_REDUCE`
  plans in addition to elementwise plans: default loop axes map to a
  flat Metal grid, the reduce body is emitted from `ScalarUop`, and
  reduce-axis `GROUP` / `GROUP_REDUCE` plus `UNROLL` candidates remain
  visible to autotune for movement-heavy lowered reductions;
- f32/f64 `S_CAST` is generated with per-input and output pointer
  types, so scalar C no longer requires one uniform kernel dtype for
  simple cast chains;
- f32/f64 `S_SHRINK`, `S_PAD`, `S_FLIP`, `S_RESHAPE`, and
  `S_RESHAPE_V` are generated by saving and rewriting loop
  coordinates around the wrapped scalar body;
- `tile_build_from_scalar` seeds a minimal plan from `scalar_uops`:

```text
TILE_LOOP_NEST(
  TILE_STORE(
    TILE_SCALAR_BODY(value expression),
    S_STORE id
  ),
  TILE_AXIS(...)
)
```

For scalar reduction values it makes the reduction explicit:

```text
TILE_LOOP_NEST(
  TILE_STORE(
    TILE_REDUCE(
      TILE_SCALAR_BODY(per-element value)
    ),
    S_STORE id
  ),
  TILE_AXIS(...)
)
```

For matmul-shaped KProg kernels, `tile_sync_from_scalar` can instead
seed a metadata-only MMA plan:

```text
TILE_MMA(
  TILE_AXIS(LOOP,   M),
  TILE_AXIS(LOOP,   N),
  TILE_AXIS(REDUCE, K)
)
```

The `TILE_MMA.extra` field stores A/B input slots and transpose flags;
`TKernelTilePlan[kid]["mma"]` exposes the decoded `TileGemmInfo`.

The scalar `S_BUFFERIZE` root still defines the complete scalar
kernel, but the tile root's body starts at the memory-write boundary:
`TILE_STORE.extra` references the scalar `S_STORE`.  On elementwise
plans, `TILE_SCALAR_BODY.extra` references the scalar value expression
stored by that `S_STORE`.  On reduce plans, `TILE_REDUCE.extra`
references the scalar `S_REDUCE_*` value and `TILE_SCALAR_BODY.extra`
references the reducer's per-element input expression.

When `KernelAxes` is present, its axis types and extents define the
`TILE_AXIS` nodes. This includes applied schedule opts such as
`UPCAST`, `UNROLL`, `LOCAL`, `GLOBAL`, `GROUP`, and `SWAP` after the
tile plan is synced. Otherwise the builder falls back to the ranges
on the scalar `S_BUFFERIZE` root.

The builder validates the emitted graph before returning success.
Malformed or partial tile arenas are allowed during manual construction
but report `tile_validate == 0` until `tile_root` points at a valid
loop nest whose body is a `TILE_STORE(TILE_SCALAR_BODY(value))` or
`TILE_STORE(TILE_REDUCE(TILE_SCALAR_BODY(value)))` chain linked back
to the scalar `S_STORE`.  Consumers should call
`tile_collect_plan_info` instead of manually walking `tile_uops` when
they need ids or axis metadata.

`materialize.c` calls the builder automatically when rangeify succeeds,
so every rangeified kernel carries a tile-plan snapshot for
introspection, CPU tile execution, and Metal tile execution.

Default dispatch ignores `tile_uops` unless `THVM_TILE=1` is set. This
is deliberate: rangeify and the scalar interpreter/JIT continue to own
correctness while the tile layer becomes a stable target for autotuning.
With `THVM_TILE=1`, supported CPU kernels route through generated tile
C or the tile interpreter; supported Metal kernels route through
generated tile MSL when the plan has one `GLOBAL` and one `LOCAL`
output axis.

## Intended Next Steps

1. Continue extending the scalar C renderer until the emitted scalar
   graph covers the same correctness surface as the scalar interpreter;
   remaining gaps are narrow/packed dtypes and bitcasts.
2. Move the conv2d/GEMV diagnostic wins into generic tile
   recognition: detect im2col-like reduce graphs as tile templates,
   expose schedule choices through `TKernelProposed`, and let
   `TKernelAutotune` / beam search pick among them.
3. Lower `TILE_REDUCE` to target-specific row-wise/group reductions
   instead of using only scalar reducer loops.
4. Generalize `TILE_MMA` target code beyond fixed 8/16/32
   threadgroup-memory shaders.  The next backend target is Metal
   `simdgroup_multiply_accumulate`; the same generic analysis should
   also be the hook for later attention-pattern discovery rather than
   adding attention-specific backend shortcuts.
