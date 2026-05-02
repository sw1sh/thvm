# Rewrite-Driven Fusion

Status: active.

Tinygrad's fusion machinery is organized around rewriteable UOps:
`UPat`/`PatternMatcher` rules canonicalize tensor graphs, rangeify
movement into explicit index expressions, insert and remove
`BUFFERIZE` nodes, compile kernels, run beam/local-size rewrites, and
then memory-plan the captured linear replay.

THVM should follow that shape.  Fusion should not live as custom
backend kernels or scattered `if` chains.  The pipeline target is:

1. Seed conservative realization boundaries.
2. Rewrite the boundary map under named legality/cost rules.
3. Rangeify each boundary into scalar index/load/store expressions.
4. Legalize scalar graphs into tile UOps.
5. Autotune tile options and replay/memory plans.

## Current Slice

`src/schedule/realize_rewrite.c` provides the first rewrite harness.
It is deliberately scoped to the realize map: `realize_classify`
still builds `REALIZE_INFO`, but boundary relaxations now run as a
named rule table with hit counters.

Current rules:

- `inline-constants`
- `inline-adjacent-reduce-chains`
- `inline-softmax-broadcast-reduce`
- `inline-reduce-scalar-tail`
- `inline-large-expand-fanout`
- `inline-pure-fanout-probe`
- `metal-tile-fanin-cap`

Set `DUMP_REWRITE=1` or `DUMP_FUSION_REWRITE=1` to print rule hit
counts during `realize_classify`.

This is not yet a full tinygrad-style graph substitution framework.
It is the first safe attachment point because the current
beautiful-mnist gap is dominated by materialization boundaries and
kernel count.  The next slice should add the same rule-table shape to
scalar graph canonicalization, where rules can rewrite movement and
index expressions before tile legality is checked.

## Rule Policy

A fusion rule must prove both:

- semantic legality: rangeify/scalar lowering can still address every
  input from the consumer edge's context;
- backend legality: the resulting scalar or tile graph can be rendered
  and replayed without falling back to generic per-op paths.

The default-off `inline-pure-fanout-probe` is the cautionary example:
duplicating a large pure movement/ALU producer can reduce nominal
dispatch count, but if the fused consumers become unsupported fat
kernels, memory and wall time regress.  Future broad recompute rules
must be guarded by scalar/tile legality, not only by UOp purity.

## Next Rules To Move Here

- `PAD` -> masked index expression for movement-heavy consumers.
- `RESHAPE`/`PAD` producer fanout with edge-local range contexts.
- Reduction-local bufferization rules once `TILE_REDUCE` is stable.
- Memory-plan rewrites over captured replay slots, sharing the same
  rule-hit diagnostics.
