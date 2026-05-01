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
- `tile_root` stores the root node id, currently a `TILE_LOOP_NEST`;
- `tile_free` is called by `kernel_free_arrays`;
- `tile_validate` checks the root/store/body/axis structure before a
  renderer or autotuner consumes the plan;
- `tile_collect_plan_info` extracts the validated root into a compact
  `TilePlanInfo` view: tile root/store/body ids, scalar store/index/
  value ids, dtype, and per-axis ids/types/extents;
- WL exposes `TKernelTileUops[kid]` for the raw tile arena and
  `TKernelTilePlan[kid]` for the validated `TilePlanInfo` view;
- `tile_loop_axis_count`, `tile_loop_axis_type`, and
  `tile_loop_axis_extent` expose root-loop axis metadata without
  assuming the root is the last emitted node; internally they only
  report data for a valid collected plan;
- tile plans remember the `KernelAxes.version` they were built
  against, and `tile_sync_from_scalar` rebuilds stale plans after
  `TKernelApplyOpt`, autotune resets, or lazy WL introspection;
- CPU dispatch has an opt-in `THVM_TILE=1` path that consumes the
  validated tile plan over scalar UOps, records dispatch kind
  `"tile"`, and falls back to the normal BLAS/JIT/scalar paths when
  no supported tile plan is present;
- that tile path first tries a generated C tile renderer for simple
  elementwise f32/f64 plans with `LOOP`/`UPCAST` axes, including
  `S_INDEX_E` addresses built from `S_I*` expression nodes, then
  falls back to the tile interpreter for broader scalar graphs such
  as reductions;
- the scalar C renderer also covers f32/f64 `S_REDUCE_SUM` and
  `S_REDUCE_MAX`; tile C dispatch still rejects tile plans carrying
  `REDUCE`/`UNROLL` axes until the tile-level reduction path lands;
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

The scalar `S_BUFFERIZE` root still defines the complete scalar
kernel, but the tile root's body starts at the memory-write boundary:
`TILE_STORE.extra` references the scalar `S_STORE`, and
`TILE_SCALAR_BODY.extra` references the scalar value expression stored
by that `S_STORE`.

When `KernelAxes` is present, its axis types and extents define the
`TILE_AXIS` nodes. This includes applied schedule opts such as
`UPCAST`, `UNROLL`, and `SWAP` after the tile plan is synced. Otherwise
the builder falls back to the ranges on the scalar `S_BUFFERIZE` root.

The builder validates the emitted graph before returning success.
Malformed or partial tile arenas are allowed during manual construction
but report `tile_validate == 0` until `tile_root` points at a valid
loop nest whose body is a `TILE_STORE(TILE_SCALAR_BODY(value))` pair
linked back to the scalar `S_STORE`.  Consumers should call
`tile_collect_plan_info` instead of manually walking `tile_uops` when
they need ids or axis metadata.

`materialize.c` calls the builder automatically when rangeify succeeds,
so every rangeified kernel carries a tile-plan snapshot even though no
backend consumes it yet.

Default dispatch ignores `tile_uops` today. This is deliberate:
rangeify and the scalar interpreter/JIT continue to own correctness
while the tile layer becomes a stable target for autotuning and future
renderers. Setting `THVM_TILE=1` routes supported CPU kernels through
the generated tile C renderer when possible, otherwise through the tile
interpreter for focused validation and profiling.

## Intended Next Steps

1. Continue extending the scalar C renderer until the emitted scalar
   graph covers the same correctness surface as the scalar interpreter;
   remaining gaps include movement wrappers, casts, and narrow/packed
   dtypes.
2. Teach the builder and renderers how to consume richer axis classes:
   `LOCAL`, `GROUP_REDUCE`, and `GLOBAL` bindings beyond the current
   introspectable `UPCAST`/`UNROLL`/`SWAP` sync.
3. Broaden the generated CPU tile renderer beyond elementwise f32/f64
   so it covers the scalar interpreter's movement and dtype surface.
4. Add generated CPU tile support for reductions instead of relying on
   the tile interpreter fallback.
5. Add a Metal tile renderer that maps `LOCAL`/`GLOBAL` axes to
   threadgroup/grid ids and uses `TILE_BARRIER`.
6. Introduce `TILE_REDUCE` for row-wise reductions and softmax-like
   kernels.
7. Add `TILE_MMA` only after reductions and local-memory tiling are
   stable.
