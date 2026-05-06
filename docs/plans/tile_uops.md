# Tile UOps Plan

Tile-level concepts (memory scope, axis types, ordering, GEMM/conv
patterns) inhabit the UOp DAG via `UOP_BUFFER` / `UOP_STORE` /
`UOP_AFTER` / `UOP_OPT` opcodes.  The renderer pattern-matches
canonical UOp shapes + OPT annotations to fire specialised
templates (e.g. `simdgroup_matrix<float, 8, 8>` for matmul-shaped
reduces with tile-aligned K extents).

See `docs/lowering_passes.md` "Pipeline today" for the architecture.
