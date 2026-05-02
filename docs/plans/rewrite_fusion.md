# Rewrite-Driven Fusion

Status: active.

## Goal

Build a tinygrad-style UOp rewrite pipeline that lowers high-level
tensor graphs into legal, autotunable scalar/tile kernels, eliminating
unnecessary materialization and kernel-count overhead for Metal
beautiful-mnist without relying on custom backend kernels.

Custom GEMM, Conv, or FlashAttention fast paths may exist as
compatibility bridges, but the primary strategy is lowered primitives
plus search.

## Acceptance Criteria

The rewrite pipeline is good enough when it can:

1. Seed conservative realization boundaries.
2. Rewrite boundary maps under named legality/cost rules.
3. Lower each boundary into scalar range/index/load/store expressions.
4. Legalize scalar graphs into tile UOps.
5. Autotune tile options.
6. Replay kernels with bounded Metal memory planning.

## Current Slice

`src/schedule/realize_rewrite.c` provides the first realize-map
rewrite harness.  It is deliberately scoped to the realize map:
`realize_classify` still builds `REALIZE_INFO`, but boundary
relaxations now run as a named rule table with hit counters.

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

`src/uop/view.c`, `src/uop/graph_rewrite.c`, and
`src/uop/graph_simplify.c` are the first UOp-level graph substitution
slices.  `uop_view` gives rewrite callbacks a stable op/source-slot
view, `uop_graph_rewrite` walks UOp DAGs bottom-up with memoization
and canonical parent rebuilds, and `uop_graph_simplify` applies the
first named symbolic rules over that core.  `uop_graph_simplify_checked`
accepts a rewrite only when shape and dtype inference prove the result
matches the input.  Set `DUMP_UOP_REWRITE=1` to print UOp rule hits
after a pass.

This is still smaller than tinygrad's full `UPat` matcher: rules are
plain C callbacks today, not declarative class/predicate captures.
That is intentional for the first implementation slice.  It gives
symbolic, movement, range, and schedule-IR rewrites one reusable
attachment point without changing current scheduling behavior.
The materializer has a default-off hook for the checked pass:
`THVM_UOP_GRAPH_SIMPLIFY=1`.

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

## Parity Checklist

Tinygrad has many declarative `PatternMatcher([...])` collections.
This checklist supports the single goal above: cover the same rewrite
families in the same pipeline positions, without copying every rule
literally.  Renderer string-format rules are lower priority than
scheduler/rangeify/codegen rules because they do not decide fusion
boundaries.

| Tinygrad rule family | Main local reference | THVM status |
| --- | --- | --- |
| Pattern infrastructure: `UPat`, `PatternMatcher`, `graph_rewrite`, matcher composition, bottom-up walk, rewrite stats | `tinygrad/uop/ops.py`, `tinygrad/uop/upat.py` | Partial. `realize_rewrite.c` names realize-boundary rules; `uop_view` and `uop_graph_rewrite` now provide UOp inspection, bottom-up traversal, memoization, parent rebuilds, replacement callbacks, and hit stats, but not declarative UPat-style captures. |
| Algebraic/symbolic simplification: constants, identities, commutative canonicalization, div/mod recombine, cast/bitcast folding, boolean/where folding | `tinygrad/uop/symbolic.py` | Partial. Constructor-time rules live in `src/uop/rewrite.c`; `uop_graph_simplify` now reuses the safe unary/binary/cast/bitcast/movement-chain subset as named graph rules, `uop_graph_simplify_checked` gates materializer use on shape/dtype preservation, and rangeify folds common scalar integer address-expression identities. Big missing piece is broader index expression canonicalization. |
| Valid-mask simplification and `WHERE`/load movement | `pm_simplify_valid`, `pm_move_where_on_load` in `tinygrad/uop/symbolic.py` | Partial. Rangeify now folds `S_IAND` identity masks, constant-condition `S_IWHERE`, and nested `WHERE(mask, WHERE(mask, value, zero), zero)` through shared emit helpers; PAD lowering sites share one canonical bounds-mask helper. General `WHERE`/load movement into pointer/index form is still missing. |
| Realize-map seeding and rangeify application | `pm_generate_realize_map`, `pm_apply_rangeify` in `tinygrad/schedule/indexing.py` | Partial. `realize_classify.c` seeds boundaries; `rangeify.c` emits scalar graphs, but not through a general rewrite table. |
| Movement-to-index rewrites | `apply_movement_op`, `pm_mops`, `pm_syntactic_sugar` in `tinygrad/schedule/rangeify.py` | Partial. THVM has edge-local rangeify fixes and view-only movement paths; `uop_graph_simplify` now has a reusable `movement-identity` table entry for no-op reshape/expand/permute/pad/shrink/flip. PAD-to-mask and full movement-to-index rules remain missing. |
| Early schedule cleanup: function/tuple resolution, copy/store hazards, reduce split, movement cleanup | `earliest_rewrites`, `mop_cleanup`, `pm_fold_moved_after` | Partial/ad-hoc. Copy/store hazards and function/multi rules are not the current beautiful-mnist bottleneck. |
| Bufferize removal and const/noop buffer folding | `pm_const_buffer_folding`, `pm_remove_bufferize`, `remove_bufferize` | Missing as a general rule family. This is one of the main fusion gaps. |
| Buffer insertion and kernel splitting | `pm_add_buffers`, `pm_add_buffers_local`, `to_define_global`, `split_kernels` | Partial. THVM materializes boundaries directly and emits scalar/tile kernels, but lacks rewriteable `BUFFERIZE`/`INDEX` nodes as first-class schedule IR. |
| Range simplification and reduce collapse | `pm_flatten_range`, `pm_simplify_ranges`, `pm_split_ranges`, `pm_reduce_simplify`, `pm_load_collapse` in `tinygrad/codegen/simplify.py` | Mostly missing. Critical for reducing movement-heavy backward kernels. |
| Upcast/unroll expansion and group-reduce local buffering | `pm_pre_expander`, `expander`, `pm_group_for_reduce` in `tinygrad/codegen/late/expander.py` | Partial. THVM tile UOps have `UPCAST`/`UNROLL`; `LOCAL`/`GROUP_REDUCE` support is still incomplete. |
| Load/store folding, devectorization, reduce-to-accumulator, add-loads | `load_store_folding`, `correct_load_store`, `devectorize`, `pm_reduce`, `pm_add_loads` in `tinygrad/codegen/late/devectorizer.py` | Partial. THVM scalar/tile renderers cover only part of this surface. |
| GPU dimension lowering | `pm_add_gpudims` in `tinygrad/codegen/gpudims.py` | Partial. Metal tile renderer maps some `GLOBAL`/`LOCAL` axes, but full grouped dimension rewrite is not there. |
| Compile/JIT rewrites: flatten linear, beam, compile, local-size optimization, exec dispatch | `pm_flatten_linear`, `pm_beam`, `pm_compile`, `pm_optimize_local_size`, `pm_exec` in `tinygrad/engine/realize.py` | Partial. THVM has autotune opts, JIT capture/replay, and graph replay, but not as one composed rewrite pipeline. |
| Memory planning rewrite | `memory_plan_rewrite` in `tinygrad/schedule/memory.py`, called from `engine/jit.py` | Partial. THVM has replay slot packing and conservative memory diagnostics; broad Metal memory planning remains unsafe. |
| Multi-device/allreduce/callify rules | `tinygrad/schedule/multi.py`, `tinygrad/callify.py` | Not a beautiful-mnist priority. Keep in inventory but do not block Metal single-device parity. |
| Renderer-specific string and dtype decompositions | `tinygrad/renderer/*`, `tinygrad/uop/decompositions.py` | Cover by need. Useful later for dtype breadth, not the current kernel-count bottleneck. |

The existing `src/rewrite/_.c` is a different system: it rewrites
equational `TAG_CTR`/`TAG_FVR` terms for the ATP layer.  It does not
replace a UOp pattern matcher because fusion rules need op classes,
shape/dtype predicates, consumer context, bottom-up graph memoization,
and cost/legality checks.

## Implementation Phases

Implement the missing rewrite families in this order under the single
goal:

1. General UOp graph rewrite skeleton: bottom-up traversal,
   replacement callback, memo, and rule stats.  Landed as
   `uop_view` plus `uop_graph_rewrite`.
2. Port the symbolic/index subset needed by rangeify: algebraic
   identities, div/mod simplification, valid-mask simplification, and
   `WHERE`/load movement.  Started with `uop_graph_simplify`, which
   lifts the existing safe constructor-time unary/binary/cast and
   reshape/expand-chain rules into the graph rewrite pipeline.  A
   default-off materialize hook (`THVM_UOP_GRAPH_SIMPLIFY=1`) now runs
   the checked pass only when shape/dtype stay stable.
3. Express movement lowering as declarative rules:
   `RESHAPE`/`PERMUTE`/`EXPAND`/`PAD`/`SHRINK`/`FLIP` over `INDEX`,
   with PAD becoming a valid mask.  Started with the
   `movement-identity` rule, which drops no-op movement nodes before
   they reach rangeify.  Rangeify also has shared valid-mask helpers
   that fold redundant `S_IAND` and constant `S_IWHERE` nodes emitted
   by PAD/RESHAPE mask paths, plus one PAD bounds-mask helper for the
   canonical `lo <= iter < hi` expression.  Nested masked values are
   flattened into one `S_IWHERE` with a combined mask, and rangeify's
   movement context transforms now have named helpers for
   `EXPAND`/`RESHAPE`/`SHRINK`/`PAD`/`FLIP`.  PAD over direct inputs
   and short scalar/movement chains now uses one edge-local
   PAD-to-index path instead of the older chain-peeling fallback.
4. Add rewriteable `BUFFERIZE`/`INDEX` schedule IR so bufferize
   insertion/removal is not hard-coded into `realize_classify`.
5. Port range/reduce simplification and reduce-to-accumulator rules.
6. Finish tile legality for `LOCAL`, `GROUP_REDUCE`, load/store
   folding, and GPU-dim lowering.
7. Wire beam/autotune and replay memory planning as rewrite passes
   over the lowered linear graph.

That checklist is the guardrail: every new fusion optimization should
land as one named rule in one of these families, with tests proving the
rule either matches tinygrad behavior or is intentionally skipped.
