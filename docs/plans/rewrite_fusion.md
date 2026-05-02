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

## Parity Checklist

Tinygrad has many declarative `PatternMatcher([...])` collections.
This checklist supports the single goal above: cover the same rewrite
families in the same pipeline positions, without copying every rule
literally.  Renderer string-format rules are lower priority than
scheduler/rangeify/codegen rules because they do not decide fusion
boundaries.

| Tinygrad rule family | Main local reference | THVM status |
| --- | --- | --- |
| Pattern infrastructure: `UPat`, `PatternMatcher`, `graph_rewrite`, matcher composition, bottom-up walk, rewrite stats | `tinygrad/uop/ops.py`, `tinygrad/uop/upat.py` | Partial. `realize_rewrite.c` is a named rule table over `REALIZE_INFO`, not a general UOp graph matcher yet. |
| Algebraic/symbolic simplification: constants, identities, commutative canonicalization, div/mod recombine, cast/bitcast folding, boolean/where folding | `tinygrad/uop/symbolic.py` | Small subset only in `src/uop/rewrite.c`. Big missing piece for index expression simplification. |
| Valid-mask simplification and `WHERE`/load movement | `pm_simplify_valid`, `pm_move_where_on_load` in `tinygrad/uop/symbolic.py` | Mostly missing. Needed before broad PAD fanout fusion is safe. |
| Realize-map seeding and rangeify application | `pm_generate_realize_map`, `pm_apply_rangeify` in `tinygrad/schedule/indexing.py` | Partial. `realize_classify.c` seeds boundaries; `rangeify.c` emits scalar graphs, but not through a general rewrite table. |
| Movement-to-index rewrites | `apply_movement_op`, `pm_mops`, `pm_syntactic_sugar` in `tinygrad/schedule/rangeify.py` | Partial. THVM has edge-local rangeify fixes and view-only movement paths, but lacks a reusable movement rewrite table. |
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

1. General UOp graph rewrite skeleton: op/class predicates, child
   captures, bottom-up traversal, replacement callback, memo, and
   rule stats.
2. Port the symbolic/index subset needed by rangeify: algebraic
   identities, div/mod simplification, valid-mask simplification, and
   `WHERE`/load movement.
3. Express movement lowering as declarative rules:
   `RESHAPE`/`PERMUTE`/`EXPAND`/`PAD`/`SHRINK`/`FLIP` over `INDEX`,
   with PAD becoming a valid mask.
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
