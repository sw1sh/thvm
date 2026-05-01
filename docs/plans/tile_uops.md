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
- `tile_free` is called by `kernel_free_arrays`;
- `tile_build_from_scalar` seeds a minimal plan from `scalar_uops`:

```text
TILE_LOOP_NEST(
  TILE_SCALAR_BODY(S_BUFFERIZE root),
  TILE_AXIS(...)
)
```

When `KernelAxes` is present, its axis types and extents define the
`TILE_AXIS` nodes. Otherwise the builder falls back to the ranges on
the scalar `S_BUFFERIZE` root.

`materialize.c` calls the builder automatically when rangeify succeeds,
so every rangeified kernel carries a tile-plan snapshot even though no
backend consumes it yet.

Dispatch ignores `tile_uops` today. This is deliberate: rangeify and
the scalar interpreter/JIT continue to own correctness while the tile
layer becomes a stable target for autotuning and future renderers.

## Intended Next Steps

1. Extend the scalar C renderer until the emitted scalar graph covers
   the same correctness surface as the scalar interpreter.
2. Teach `tile_build_from_scalar` to preserve more schedule structure:
   `UPCAST`, `UNROLL`, `LOCAL`, `GROUP_REDUCE`, and `GLOBAL`.
3. Add a CPU tile renderer that lowers `TILE_LOOP_NEST` to nested
   loops and honors `UPCAST`/`UNROLL`.
4. Add a Metal tile renderer that maps `LOCAL`/`GLOBAL` axes to
   threadgroup/grid ids and uses `TILE_BARRIER`.
5. Introduce `TILE_REDUCE` for row-wise reductions and softmax-like
   kernels.
6. Add `TILE_MMA` only after reductions and local-memory tiling are
   stable.
