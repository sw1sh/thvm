# Bufferize Schedule IR Plan

Status: Phases 0-7 landed at the data-layer level.  Phase 2 has the
data layer + edge query API; Phase 4 has the cost-model fields and
candidate dump; Phase 5 has the reduce-aware cost gate; Phase 6 has
per-buffer lifetimes and output_bytes; Phase 7 has the schedule key
and aggregates.  All seven phases are observable from
`DUMP_BUFFERIZE` and `DUMP_BUFFERIZE_CANDIDATES`.

Outstanding follow-ups (each tracked in its phase section below):

- rangeify rerouting through `B_INDEX` (Phase 2);
- edge transforms beyond accounting (Phase 3);
- named removal rules on the bufferize graph (Phase 4);
- accumulator/group-reduce schedules (Phase 5);
- rerouting Metal allocation through bufferize lifetimes (Phase 6);
- schedule cache + replay using the key (Phase 7).

These follow-ups are where actual runtime/memory wins come from;
the data layer just unlocks them.

## Goal

Build a first-class `BUFFERIZE`/`INDEX` schedule IR that sits between
the high-level UOp graph and scalar/tile lowering.  The IR should make
materialization boundaries explicit, rewriteable, inspectable, and
autotunable, so Metal beautiful-mnist can converge toward tinygrad-like
fusion and memory behavior without relying on custom backend kernels.

The current `REALIZE_INFO` table is useful but too implicit: it records
which UOp heap locations survive as buffers, while rangeify and
materialize still infer consumer edge contexts from the original graph.
The new IR must preserve the useful conservative seeding model while
making every buffer, index, valid mask, edge context, and rewrite
decision visible as data.

## Target Shape

The pipeline should become:

1. High-level UOp graph from WL/interactions.
2. UOp graph simplification: algebraic, movement, dtype, and mask
   rewrites that are safe independent of consumer context.
3. Bufferize schedule graph:
   conservative `BUFFERIZE` nodes, edge-local `INDEX` nodes, explicit
   stores, reason bits, and legality metadata.
4. Bufferize rewrites:
   remove, insert, split, merge, and fold buffer boundaries under named
   semantic and backend legality rules.
5. Rangeify:
   lower each final store from edge-local `INDEX` expressions into
   scalar range/index/load/reduce/store graphs.
6. Tile legalization:
   map scalar loops/reduces to tile axes, local/group-reduce axes, GPU
   dimensions, and memory scopes.
7. Autotune and replay:
   run beam/search over legal tile options, then replay with bounded
   Metal memory planning.

No public WL API change is required.  This is a scheduling/lowering
internal refactor.

## Core IR Concepts

The initial C representation can be structs rather than new heap tags.
The important requirement is that dumps and tests see one explicit graph
instead of recovering schedule state indirectly from `REALIZE_INFO`.

### `B_VALUE`

Wraps an existing UOp term.  It is the pure value being scheduled.

Fields:

- source UOp term and heap loc;
- inferred shape and dtype;
- op class bits: elementwise, movement, reduce, load, const, view-only,
  side-effecting, backend-specific;
- estimated cost: op count, input count, output bytes, reduce flops.

### `B_BUFFERIZE`

Declares that a value has a materialized backing buffer.

Fields:

- value id;
- stable buffer id;
- shape, dtype, byte size;
- reason bits: root, multi-consumer, reduce, copy/hazard, backend cap,
  user-forced, memory-plan split, autotune split;
- memory class: global, local, alias, external, constant;
- lifetime metadata: first use, last use, can alias, can recompute;
- rewrite history: which rule inserted, removed, or preserved it.

This generalizes the current `UOpInfo.realized` plus reason bits.

### `B_INDEX`

Represents a consumer edge reading a value through a context.

Fields:

- source value or buffer id;
- consumer id;
- logical consumer ranges;
- source index expression;
- valid mask expression;
- movement chain summary;
- edge-local dtype/cast metadata;
- legality flags: direct load, masked load, scalar expression load,
  reduction input, tileable input.

This is the missing piece today.  It prevents one global `prog_value[i]`
or first-seen input-slot range from standing in for all consumer views.

### `B_STORE`

Represents one final kernel output.

Fields:

- destination `B_BUFFERIZE`;
- scalar expression root;
- output ranges;
- reduce ranges;
- store mask;
- backend constraints: max inputs, max scalar ops, local memory bytes,
  threadgroup size, graph replay compatibility.

`B_STORE` is what rangeify should lower, not an arbitrary high-level UOp
root plus side-channel `REALIZE_INFO`.

## Boundary Reasons

Keep the current conservative seeding, but express each boundary as a
reasoned `B_BUFFERIZE`.

Required initial reasons:

- `ROOT`: caller-requested result escapes.
- `MULTI`: value has two or more distinct consumers.
- `REDUCE`: value is a reduce result that may need an accumulator or
  separate kernel.
- `COPY_HAZARD`: source/destination aliasing or mutation hazard.
- `BACKEND_CAP`: direct fused kernel exceeds Metal or C renderer limits.
- `RANGEIFY_GAP`: scalar graph cannot yet express an edge context.
- `TILE_GAP`: scalar graph is valid but tile legalization cannot render
  it profitably.
- `MEMORY_SPLIT`: replay memory planner intentionally splits a lifetime.
- `USER_FORCED`: debugging or API-level force-realize.

Required rewrite outcomes:

- `PRESERVE`: boundary stays for semantic or backend reasons.
- `REMOVE`: boundary is recomputed or folded away.
- `MERGE`: adjacent boundaries collapse into one store.
- `SPLIT`: one fat boundary becomes multiple legal stores.
- `ALIAS`: boundary becomes an alias/view instead of allocation.

Every reason and outcome must be dumpable and testable.

## Rule Families

Rules should be named and grouped by where they run.  A rule can only
land when it has a focused C or WL test and a diagnostic counter.

### Seed Rules

These add initial `B_BUFFERIZE` nodes.

- `seed-root-buffer`
- `seed-multi-consumer-buffer`
- `seed-reduce-buffer`
- `seed-copy-hazard-buffer`
- `seed-backend-cap-buffer`
- `seed-rangeify-gap-buffer`
- `seed-tile-gap-buffer`

The first implementation can mirror the current `realize_classify`
result exactly.  Behavior changes should come only after the generated
bufferize graph is dumpable.

### Removal Rules

These remove unnecessary materialization.

- `remove-const-bufferize`: constants inline into consumers.
- `remove-single-use-bufferize`: single-use pure producers fuse into
  the only consumer.
- `remove-removable-bufferize`: current default Metal/tile rule for
  small pure multi-consumer fanout.
- `remove-noop-view-bufferize`: reshape/expand/permute/pad/shrink/flip
  that can be represented as `INDEX`.
- `remove-broadcast-reduce-bufferize`: softmax/batchnorm-style reduce
  followed by broadcast consumers when local/group reduction can cover
  the consumer edge.
- `remove-scalar-tail-bufferize`: scalar reduce tails that can be
  fused into final scalar output.

Each removal rule must prove:

- all consumers can be expressed through edge-local `B_INDEX`;
- recompute cost is within a budget, or autotune selected it;
- resulting store stays under backend input/op/local-memory limits;
- memory pressure does not increase past a measured threshold.

### Insertion And Split Rules

These add boundaries when fusion is legal but not profitable or not
renderable.

- `insert-fanin-cap-buffer`: split kernels that exceed Metal direct
  buffer count.
- `insert-op-budget-buffer`: split scalar graphs that exceed renderer
  or compile-time limits.
- `insert-reduce-accumulator-buffer`: allocate accumulator boundaries
  for reductions that cannot be held in registers/local memory.
- `insert-local-buffer`: introduce local/threadgroup buffers for tile
  schedules.
- `insert-replay-lifetime-split`: split long-lived temporaries when
  replay memory pressure is worse than recompute.

These rules are as important as removal.  Tinygrad-style fusion quality
comes from moving boundaries to good positions, not just deleting them.

### Movement-To-Index Rules (Phase 3 - landed as accounting layer)

These rewrite consumer edges, not producers globally.

- `index-reshape` (landed accounting + identity-reshape elision)
- `index-permute` (landed accounting + identity-permute elision)
- `index-expand` (landed accounting + identity-expand elision)
- `index-pad-mask` (landed accounting)
- `index-shrink` (landed accounting)
- `index-flip` (landed accounting)
- `index-contiguous-collapse` (planned)
- `index-load-mask-to-valid` (planned)

The first six rules exist as named accounting rules; each one
increments its hit count for every B_INDEX edge carrying the
matching `has_*` flag, and `DUMP_BUFFERIZE=1` prints non-zero
rule lines as `index_rule <name> hits=<n>`.  Identity-reshape
elision is the first one that actually transforms an edge: the
`bufferize_apply_identity_reshape` pass runs after edge construction
and drops chain entries whose `src_dims == out_dims`, decrementing
`movement_chain_len` and clearing `has_reshape` when no surviving
reshape op remains on the edge.  Rangeify still owns the
underlying movement-to-index codegen via `RngsCtx` for non-identity
chains; future transforms will collapse adjacent permutes, fold
expand-of-broadcast, and lift PAD masks once the per-op pad/perm
data is captured on `BIndexChainOp`.

`PAD` must become an index transform plus valid mask, not a separate
buffer unless a later legality rule proves materialization is cheaper.
The same producer may have several different `B_INDEX` edges if several
consumers see it through different movement contexts.

### Symbolic Index Rules

These simplify index and mask expressions after movement lowering.

- constant folding for add/mul/div/mod/comparison;
- affine normalization;
- `div/mod` recombination;
- mask identity and annihilator folding;
- nested `WHERE` mask flattening;
- load valid-mask movement;
- common-subexpression extraction inside one store.

The target is smaller scalar address graphs before rangeify emits
`S_INDEX_E`, `S_IDIV`, `S_IMOD`, and mask `S_IWHERE` nodes.

### Reduce Rules

These handle the largest remaining beautiful-mnist kernels.

- `reduce-chain-collapse`: keep explicit source reduce axes instead of
  materializing adjacent reductions.
- `reduce-to-accumulator`: turn reduce ranges into accumulator loops or
  local/group reductions.
- `reduce-broadcast-fuse`: fuse reduce results into broadcast consumers
  when valid and profitable.
- `reduce-split-axis`: split very large reductions for local memory,
  occupancy, or graph replay constraints.
- `reduce-softmax-row`: express row-wise max/sum/normalize patterns in
  general reduce IR, not as a custom backend kernel.

`TILE_REDUCE` should consume the same metadata as these rules.  Do not
add a FlashAttention or Conv custom path as the main solution; those are
only compatibility bridges.

### Memory Rules

These run after candidate kernels are known.

- `memory-alias-view`: avoid allocation for pure aliases/views.
- `memory-reuse-dead-slot`: reuse dead temporary buffers.
- `memory-pack-larger-slot`: reuse larger retained Metal buffers for
  smaller logical temporaries.
- `memory-preserve-batch`: preserve all outputs reachable from a
  captured replay batch.
- `memory-recompute-vs-store`: choose recompute when it reduces peak
  retained memory and does not increase runtime too much.

No broad autotune sweep should run until these rules are proven on
bounded canaries.  Memory planning must be correct before search expands
candidate count.

## Legality Model

Every rewrite must answer four questions before it can change behavior.

1. Semantic legality:
   shape, dtype, valid mask, reduce axes, and aliasing match the
   original UOp graph.
2. Rangeify legality:
   every consumer edge can emit scalar index/load/store expressions from
   its own `B_INDEX` context.
3. Backend legality:
   scalar C, CPU interpreter, Metal tile, and graph replay either render
   the result or intentionally decline it before boundary removal.
4. Cost legality:
   recompute, op count, input count, local memory, output bytes, and
   replay lifetime estimates are inside rule budgets or selected by
   autotune.

When a rule declines, it should record a skip reason:

- `shape`
- `dtype`
- `alias`
- `rangeify`
- `tile`
- `input-cap`
- `op-budget`
- `reduce-context`
- `memory-pressure`
- `unsupported-op`

Skip reasons are necessary because beautiful-mnist has many repeated
patterns.  A dump should answer whether a rule is not matching, matching
but illegal, or matching and losing the cost model.

## Diagnostics

Add these dumps before changing behavior broadly:

- `DUMP_BUFFERIZE=1`: one-line schedule graph summary.
- `DUMP_BUFFERIZE_IR=1`: full `B_BUFFERIZE`/`B_INDEX`/`B_STORE` graph.
- `DUMP_BUFFERIZE_REWRITE=1`: rule hit and skip counts.
- `DUMP_BUFFERIZE_CANDIDATES=1`: top removable and splittable
  boundaries with cost estimates.
- `DUMP_BUFFERIZE_EDGE=K`: show edge-local index/mask lowering for one
  buffer or kernel id.
- `DUMP_MEMORY_IR=1`: include bufferize ids in replay lifetime dumps.

The dumps should include stable ids, op names, shapes, dtypes, byte
sizes, reason bits, final outcome, and backend legality flags.  They
must be compact enough to use on a one-step beautiful-mnist capture.

## Implementation Phases

### Phase 0: Freeze Current Behavior Behind The New IR (landed)

Build a bufferize graph from the existing `REALIZE_INFO` result and
prove it emits the same materialized kernels as today.

Status: landed in `src/schedule/bufferize.c`.  `realize_classify`
projects every realized UOp into a `B_BUFFERIZE` record with a stable
1-based buffer id, the realize root gets one `B_STORE`, and reasons
are mirrored as `BUFFERIZE_REASON_{ROOT,MULTI,REDUCE,BACKEND_CAP}`.
`DUMP_BUFFERIZE=1` prints the table.  `tests/test_bufferize.c`
asserts the projection is faithful (every realized loc appears
exactly once, no inlined locs appear, ids are dense, the root has a
matching store).  `B_VALUE` / `B_INDEX` are intentionally still TODO
because the existing `realize_classify` result has no edge-local
context to project from; both arrive together with Phase 2.

Acceptance:

- no change to kernel count or WL results (materialize.c still reads
  `REALIZE_INFO`; bufferize is a read-only mirror);
- `make test` passes including the new `tests/test_bufferize.c`.

### Phase 1: Move Current Boundary Rewrites Into Bufferize IR (landed)

Port the existing named realize-map rules so they operate on explicit
bufferize nodes.

Status: landed.  The bufferize graph is now seeded from
`REALIZE_INFO` between the ROOT/MULTI/REDUCE marking pass and
`realize_rewrite_apply`, and stays live across the rewrite phase.
`realize_rewrite_apply` sets `bufferize_set_current_rule(name)` around
each rule's `apply` callback; `realize_mark` and `realize_unmark`
forward into `bufferize_realize_with_reason` and
`bufferize_unrealize`, which stamp `added_by` / `removed_by` from
that pointer.  All existing realize-map rules
(`inline-constants`, the adjacent reduce-chain and scalar-tail
rules, `remove-removable-bufferize`, `metal-tile-fanin-cap`, the
fanout probes) keep their current shape and rule names; the
forwarding wiring means each one's effect is now visible on the
explicit graph without porting per-rule code.  Materialize.c still
reads `REALIZE_INFO`, so behaviour is unchanged.

Acceptance:

- `make test` passes including the new `inline-constants`
  removed-by stamp test;
- all existing rewrite stats preserved with their original names;
- disable flags still bisect behavior identically.

Follow-up before Phase 2: rules that allocate fresh consumer-count
data through `realize_info_find` after a forward should also push
that data through the bufferize API; today brand-new buffers added
by `metal-tile-fanin-cap` re-read `REALIZE_INFO` for their
consumer_count.  Once `B_INDEX` lands in Phase 2, the bufferize
graph will be the only authoritative source.

### Phase 2: Make `B_INDEX` Edge-Local And Mandatory

Stop relying on producer-global movement context for consumer loads.

Status: data layer landed.  `bufferize_finalize_stores` now builds
one `B_INDEX` per producer-buffer to consumer-buffer edge, walking
each consumer's compute tree, descending transparently through
unrealized (rule-removed) buffers, and stopping at every realized
producer.  Each record carries `movement_chain_len` plus
`has_reshape`/`has_permute`/`has_expand`/`has_pad`/`has_shrink`/
`has_flip` flags so different consumers of the same producer get
distinct edge entries with their own movement contexts.
`tests/test_bufferize.c` covers a multi-consumer no-movement case,
a RESHAPE-on-one-branch case, and the
`bufferize_indexes_for_consumer` helper.

Tasks:

- ~~generate one `B_INDEX` for every producer-to-consumer edge~~;
- ~~encode reshape/pad/expand/shrink/flip/permutation context on the edge~~;
- ~~expose an edge query API (`bufferize_edge_summary`) so rangeify
  and materialize can consult B_INDEX directly~~;
- ~~add focused tests for shared producer with multiple movement views~~;
- ~~capture per-op source/output dims on each chain entry~~;
- ~~capture pad widths, axis perms, and flip masks on each chain
  entry~~ (BIndexChainOp now fully describes each movement op);
- ~~materialize-side wiring + per-input-slot link to a bufferize
  edge~~ (`KernelEntry.input_source_buffer_ids[]` + accessors
  `kernel_entry_input_source_buffer_id` and
  `kernel_entry_input_edge_summary`);
- ~~per-USE `KProgOp.chain_op_idx` field populated during `visit()`~~;
- ~~per-USE `KProgOp.chain_edge_idx` for multi-path
  disambiguation~~;
- ~~identity-aware `prog_chain_propagate` so chain depth stays
  aligned across `bufferize_apply_identity_reshape` elision~~;
- ~~kernel-root op gets no BIndex chain entry (math handled by
  the in-range check)~~;
- ~~reroute `RngsCtx` chain construction in `schedule/rangeify.c`
  through `kernel_entry_prog_chain_op` so the canonical chain
  drives codegen instead of `KProgOp.src0_dims`/`out_dims`/
  `pad_widths`/`axis_perm`~~ (landed: all six `rngs_ctx_*`
  movement composers go through a `rngs_chain_shape` helper that
  prefers `BIndexChainOp` when the slot has a single-leaf chain
  with a non-broken `chain_input_slot`);
- preserve valid masks through `S_IWHERE` from `has_pad` rather than
  rederiving them inside `rngs_ctx_pad` (pending - same per-USE
  prerequisite).

Until the rerouting lands, the graph data is informational and
queryable; rangeify's existing per-USE addressing already produces
correct edge-local code via `KProgOp`.  Cost-model and removal
rules in Phases 4+ can already act on the data.

Acceptance (data layer + query API):

- `make test` passes including the new edge-table and
  edge-summary cases.

Acceptance (rangeify rerouting, pending):

- current `rangeify_gaps.wlt`, `grad.wlt`, and `nn.wlt` still pass with
  `THVM_RANGEIFY_BAIL=1`;
- no regression in LeNet/attention movement-heavy cases.

### Phase 3: General Movement-To-Index Rewrite Rules

Replace movement special cases with named edge rewrite rules.

Tasks:

- implement `index-pad-mask`, `index-reshape`, `index-expand`,
  `index-shrink`, `index-flip`, and `index-permute`;
- add symbolic mask simplification after each edge rewrite;
- remove old chain-peeling paths only after no-bail tests cover them;
- dump edge-local source indices for failed or skipped rewrites.

Acceptance:

- fewer `RANGEIFY_GAP` boundaries;
- no new `metal-jit` fallbacks in beautiful-mnist;
- scalar interpreter and C scalar renderer agree on movement tests.

### Phase 4: First-Class Bufferize Removal (cost-model layer landed)

Implement tinygrad-style `INDEX(BUFFERIZE(x)) -> INDEX(x)` rewrites
under legality gates.

Status: cost-model fields and the candidate dump have landed.
`schedule/bufferize.c` populates `recompute_ops`, `output_numel`,
and `recompute_total` per realized buffer, and
`bufferize_removal_score(buffer_id)` returns a first-cut score
(`output_numel / max(recompute_total, 1)`) that is zero for ROOT,
REDUCE, and BACKEND_CAP buffers.  `DUMP_BUFFERIZE_CANDIDATES=1`
prints the top 20 candidates sorted by score.  No removal rules
change behavior yet - the cost model is informational - but the
data is now what future rules will read.

Tasks:

- ~~add cost model inputs: op count, bytes, consumers, reduce
  depth, backend input count, local memory~~ (op count, bytes via
  numel, consumers, recompute_total all done; reduce depth and
  backend input count remain);
- ~~add candidate dump sorted by potential kernel-count reduction
  and memory impact~~;
- remove const/noop view buffers (planned - extends the existing
  `inline-constants` realize-rule into a bufferize-graph rule);
- remove pure single-use buffers (planned);
- remove small pure multi-consumer buffers by recompute - first
  bufferize-graph-driven rule landed as `remove-by-cost-score`,
  default-ON with conservative threshold (1000 bytes per recompute
  op) and op-budget caps matching `remove-removable-bufferize`.
  Set `THVM_BUFFERIZE_REMOVE_BY_SCORE=0` to disable.  The rule
  iterates `bufferize_buffer_at`, reads
  `bufferize_removal_score`, and calls `realize_unmark` only to
  project the decision back for materialize.  Coexists with the
  existing `remove-removable-bufferize` realize-rule (different
  threshold semantics: this one rewards memory-savings ratio
  rather than absolute op count).

Acceptance (data layer):

- `make test` passes including the new cost-model cases.

Acceptance (removal rules, pending):

- one-step beautiful-mnist kernel count drops materially from the
  current 1330-kernel canary;
- retained/live Metal memory does not grow;
- performance is not slower by more than noise on the bounded canary.

### Phase 5: Reduce-Aware Bufferize (cost gate landed)

Handle reductions as accumulator schedules, not unconditional
materialization points.

Status: data-layer reduce gate landed.  `BBufferize.subtree_has_reduce`
is set during cost computation when the producer-subtree walk hits
a REDUCE; `bufferize_removal_score` returns 0 for any buffer with
that flag set, on top of the existing reason gates.  This blocks
the simple recompute heuristic from removing buffers whose
recompute path crosses a reduce.  Future reduce-aware rules can
relax this when they can fold the reduce into the consumer (local
group reduce, broadcast fusion).

Tasks:

- ~~track reduce presence in the cost model so simple recompute
  removals do not propose reduce-bearing candidates~~;
- ~~tinygrad rule port: generalised broadcast-reduce predicate
  (Phase 1 of port -- `realize_rule_inline_softmax_broadcast_reduce`
  now matches BatchNorm/softmax chains)~~;
- ~~tinygrad rule port: `reduce-const-mul-distribute` UOp rule --
  `REDUCE_SUM(MUL(x, CONST))` -> `MUL(REDUCE_SUM(x), CONST)` lives in
  `uop_graph_simplify` so the constant rides on the post-reduce
  shape~~;
- ~~tinygrad rule port: `reduce-add-const-distribute` UOp rule --
  `REDUCE_SUM(ADD(x, CONST))` -> `ADD(REDUCE_SUM(x), CONST * N)`
  with `N` the reduced axis extent; folds the broadcasted scalar
  add over the reduce input into one post-reduce scalar add~~;
- represent accumulator buffers explicitly (planned);
- fuse reduce chains through `B_STORE` reduce ranges (planned -
  the existing `inline-adjacent-reduce-chains` realize-rule does
  this; the bufferize-graph version needs explicit reduce-axis
  metadata on `B_STORE`);
- add `TILE_REDUCE` legalization over explicit reduce metadata
  (planned);
- introduce local/group-reduce axes before MMA work (planned);
- implement reduce-broadcast fusion for batchnorm and softmax-like
  patterns (planned - the existing
  `inline-softmax-broadcast-reduce` realize-rule covers some
  cases; the bufferize-graph version generalises it).

Acceptance (cost gate):

- `make test` passes including the reduce-aware cost-gate cases.

Acceptance (accumulator schedules, pending):

- batchnorm channel reductions and beautiful-mnist backward reduce
  patterns stop dominating kernel count;
- Metal tile remains the dispatch path, not `metal-jit`;
- local/group reduce options are visible to autotune.

### Phase 6: Memory Planning Over Bufferize IR (data layer landed)

Make memory planning consume schedule graph lifetimes instead of only
captured replay side effects.

Status: data-layer lifetime tracking landed.  Each realized
B_BUFFERIZE now carries `lifetime_start` and `lifetime_end`
(topological depths from the B_INDEX edge graph) plus
`output_bytes` (`output_numel * dtype_itemsize`).
`bufferize_buffer_lifetime(buffer_id, *start, *end)` exposes the
data and `DUMP_BUFFERIZE` shows `lifetime=<start>..<end>`
alongside the cost lines.  Metal allocation still consumes
materialize.c's BOUNDARY_DEPTH / BOUNDARY_LAST_USE; the bufferize
lifetimes are computed independently and serve as the canonical
record future memory-planning rules will read.

Tasks:

- ~~compute first/last use from `B_STORE` and replay graph~~ (done
  for the bufferize-graph side; the replay-graph reconciliation
  comes when materialize switches over);
- choose alias/reuse/recompute candidates before Metal allocation
  (planned - extends the existing kernel_alloc / kernel_gc code
  with bufferize-lifetime queries);
- preserve all outputs in a replay batch boundary (planned);
- add memory pressure skip reasons to bufferize rewrites (planned -
  adds a `BUFFERIZE_REASON_MEMORY_SPLIT` and a per-buffer
  `memory_pressure_blocks_removal` flag);
- keep broad sweeps disabled until bounded memory canaries are
  stable.

Acceptance (data layer):

- `make test` passes including the lifetime + output_bytes cases.

Acceptance (Metal allocation, pending):

- beautiful-mnist one-step retained memory moves toward tinygrad scale;
- no 100 GB pressure failure under bounded canaries;
- replay correctness survives multi-output and multi-grad graphs.

### Phase 7: Autotune Integration (key + aggregates landed)

Let autotune choose among legal bufferize/tile alternatives instead of
only choosing per-kernel axis opts.

Status: schedule-key and aggregate accessors landed.
`bufferize_schedule_key()` returns a deterministic FNV-1a hash over
each realized buffer's loc-independent fields and each B_INDEX
edge, so the key stays stable across runs that produce the same
schedule and flips when the shape changes.
`bufferize_total_realized_bytes`, `bufferize_max_lifetime_depth`,
and `bufferize_total_recompute_ops` give one-number summaries
autotune can use to pre-filter candidates without walking every
buffer.  `DUMP_BUFFERIZE` shows
`key=0x<hex> total_bytes=<n> max_depth=<n>`.

Tasks:

- ~~encode candidate schedules with stable keys~~;
- compare remove-vs-preserve-vs-split alternatives (planned -
  needs the rule-driven schedule mutations from Phases 3-5 to
  actually generate alternatives);
- feed tile options and memory estimates into one score (planned);
- cache winning schedule choices by graph/kid/key (planned -
  the key API exists; the cache/lookup wiring is autotune-side
  work);
- replay the selected schedule without re-searching (planned).

Acceptance (data layer):

- `make test` passes including the deterministic-key, key-flips,
  and aggregate-totals cases.

Acceptance (autotune cache, pending):

- bounded beautiful-mnist training loop can run autotune without
  memory blow-up;
- selected schedules reduce full-loop wall time, not just
  first-sample or compile-time metrics;
- canary output reports kernel count, graph dispatch count,
  memory, and training-step wall time after each run.

## Tests

Add tests in this order:

- C unit tests for graph construction and reason projection.
- C unit tests for each rewrite rule hit and skip reason.
- C scalar/rangeify tests for edge-local `B_INDEX` movement contexts.
- WL no-bail tests for movement-heavy grad, nn, attention, and LeNet
  cases.
- WL Metal tests that assert no fallback to generic `metal-jit` for
  selected beautiful-mnist kernels.
- Bounded beautiful-mnist canary:
  `BS=32 WARMUP_STEPS=1 N_STEPS=1 POST_AUTOTUNE_TOP=6`.
- Full training-loop benchmark only after memory planning is stable.

Do not run broad autotune sweeps until Phase 6 has bounded memory
proofs.

## Multi-output kernel infrastructure (Phase 5+ groundwork)

The bigger algorithmic gap to tinygrad-level fusion is that
tinygrad's beautiful-mnist JITs to ~3 megakernels per training
step (forward+loss, backward, optimizer); each megakernel writes
many output buffers from a single fused loop.  thvm's
`KernelEntry` has historically held a single `output_tid`, so
even after all the bufferize-graph rewrites here every realize
boundary lands in its own kernel — currently 1070 dispatches
per step.

Closing this gap requires multi-output kernels.  The work splits
into independent landings:

1. **Schema (landed)**.  `KernelEntry` gains
   `n_extra_outputs` plus `extra_output_{tids,dtypes,shapes,
   numels}[KERNEL_MAX_EXTRA_OUTPUTS]`.  Output index 0 stays on
   the legacy `output_tid` family for backward compatibility;
   indices `1..n_extra_outputs` come from the extras arrays.
   Public accessors `kernel_entry_output_count`,
   `kernel_entry_output_tid_at`, `kernel_entry_output_dtype_at`,
   `kernel_entry_output_numel_at`, `kernel_entry_output_shape_at`,
   and `kernel_entry_set_extra_output` give every consumer
   (codegen, dispatch, autotune, JIT capture) a single canonical
   access path.  Existing single-output code unchanged - new
   tests in `test_bufferize.c` confirm `n_outputs == 1` and that
   `set_extra_output(slot=0)` is rejected.

2. **Materialize-side kernel-merging pass (planned)**.  After
   `topo_sort_boundaries` builds `BOUNDARY_ORDER`, scan for
   pairs of boundaries that share:
   - identical iter shape (same output rank+dims),
   - identical input set (or near-identical, modulo dedup),
   - no data-flow dependency (neither reads the other),
   - both pure-elementwise or pure-elementwise-with-tail-reduce,
     so the fused program stays tile-feasible.
   Merge the two `KProgOp` programs by remapping the second
   kernel's output slots into `n_extra_outputs` of the first,
   then drop the second from `BOUNDARY_ORDER`.

3. **Codegen rendering (planned)**.  `render_metal_tile`,
   `render_c`, and `render_c_scalar` need to emit kernels with N
   output buffer arguments and N store statements.  The
   ScalarUop graph already has `S_DEFINE_OUTPUT` and `S_STORE`
   per output; rangeify just needs to emit them per-output.

4. **Dispatch (planned)**.  `metal_tile_jit_encode` and
   `metal_jit_encode` need to bind N output buffers; the CPU
   interpreter needs to set up N output pointer args.  JIT
   capture's per-op output tracking already iterates `output_tid`
   — bumping to `kernel_entry_output_count` is the surface
   change.

5. **Autotune (planned)**.  Multi-output kernels widen the
   schedule space; `bufferize_schedule_key` already covers
   reasons + chain summaries, but per-output reasons / sizes
   should hash into the key so autotune treats fused vs unfused
   schedules as distinct.

Each step is independent and testable in isolation.  Step 1's
schema is the prerequisite; without it, every other step would
need to re-design the storage.

## Scalar UOp Simplification Harness (Phases 2-3 of tinygrad rule port)

Status: harness landed (Phase 2); Phase 3 lands the first divandmod
rules.  `src/scalar/simplify.c` ships `scalar_simplify_apply` -- a
bottom-up rewrite driver over the per-kernel ScalarUop[] arena that
mirrors the shape of `uop_graph_rewrite` (rule struct, named hits,
`DUMP_SCALAR_SIMPLIFY=1` stats dump).  Materialize calls it after
`rangeify_dce` / `axes_ensure_scalar_reduce` and before tile sync,
gated by `THVM_SCALAR_SIMPLIFY` (default on).

Rules receive `ScalarUop **` and `u32 *` arena handles plus a
`KernelEntry *` user pointer so they can allocate replacement nodes
via `rangeify_emit_*`; the driver re-reads the arena head after each
rule call to follow the realloc.

### Phase 3: divandmod rules (port of tinygrad/uop/divandmod.py)

Landed (in `scalar_simplify_divandmod_rules`):

- `nested-div-mod`: `(x % (k*c)) // c` -> `(x // c) % k`
  (divandmod.py:26-27, IDIV branch).  Collapses cascaded mod/div
  expressions that arise from reshape o stride composition.
- `nested-div-mod-mod`: `(x % (k*c)) % c` -> `x % c`
  (divandmod.py:26-27, MOD branch).  Same pattern, MOD outer.
- `add-div-split`: `(x + c) // d` -> `(x + (c % d)) // d + (c // d)`
  when `c >= d`, x non-negative, d > 0 (divandmod.py:112-113).
  Hoists the integer part of c so a downstream constant-folder can
  merge it with sibling constants.

Conditions on numerator non-negativity are answered by the
structural `simplify_value_nonneg` estimator (S_RANGE iters,
non-negative S_ICONSTs, and IADD/IMUL/IDIV/IMOD compositions thereof
with positive-constant denominators).

TODO (in priority order, mirroring `divandmod.py`):

- `factor-remainder` (divandmod.py:91-104): `(d*x + y) // d`
  -> `x + (y // d)` when y, x non-negative.
- `gcd-remainder` (divandmod.py:58-63): factor common gcd from
  numerator and denominator.
- `fast-inline-div` (divandmod.py:108-109): `(x // c + a) // d`
  -> `(x + a*c) // (c*d)`.
- `mod-flip`, `mod-flip-div`, `neg-div-flip` (divandmod.py:110,
  121-122): sign-flip rewrites for x.vmax <= 0 / d.vmax < 0.
- `fold-divmod-general` (divandmod.py:8-104): the slow-path
  polynomial decomposition (`cancel_divmod`, congruence folding,
  nest-by-factor).  Requires a richer value-range estimator.

`tests/test_scalar_simplify.c` covers each landed rule with both
a positive (firing) and negative (skipped because guard fails)
case, and a structural-shape assertion on the rewritten graph.

Canary impact (`BS=32 WARMUP_STEPS=1 N_STEPS=1 POST_AUTOTUNE_TOP=6`,
beautiful-mnist train): rules wired but 0 hits across 94 kernels --
post-Phase-1 beautiful-mnist's index expressions don't surface
nested-mod patterns at the scalar layer.  The rules are foundation
for harder workloads (deeper reshape chains, non-elementwise convs)
where divandmod patterns survive past CSE/DCE.

## Current Baseline

Latest bounded Metal/tile beautiful-mnist canary
(`BS=32 WARMUP_STEPS=1 N_STEPS=1 POST_AUTOTUNE_TOP=6`) after Phase
1 of the tinygrad rule port (broadcast-reduce predicate
generalisation) plus the metal-op fallback fix for REDUCE-rooted
absorbing kernels (`THVM_BUFFERIZE_SOFTMAX_REDUCE_TILE_CAP`):

- timed step: `84.9 ms` (vs `694 ms` original reference, -87.8%;
  vs `168.4 ms` pre-fix Phase-1 baseline, -49.6%);
- kernels: `118` (vs `1330` original reference, -91.1%; vs `113`
  pre-fix Phase-1 baseline, +5 -- the absorbing-REDUCE gate keeps
  five extra BN REDUCEs realized);
- dispatch: `metal-tile=108`, `metal-op=10` (vs `86/27` pre-fix);
  the remaining 10 metal-op fallbacks bail rangeify on
  `pre-reduce dim mismatch` rather than the `> 1 reduce` envelope;
- peak retained Metal memory: `171 MB` (vs `1881 MB` pre-Phase-1
  baseline, -90.9%; vs `392 MB` pre-fix, -56.4%).

Phase 1 alone closed most of the kernel-count gap.  The chain
predicate now matches BatchNorm-train / BN-grad shapes via:

- `realize_term_is_broadcast_of_const` walks through
  `EXPAND/RESHAPE/PERMUTE/CAST/BITCAST` to bottom out at a CONST
  (so the WL `t / N` lowering's `MUL(t, EXPAND(CONST))` form is
  recognised as having a const sibling);
- RESHAPE/PERMUTE are valid scalar-preserving chain hops;
- multi-consumer intermediates fan the chain check to every
  parent (entry-point predicate is now always multi-parent and
  strictly subsumes the legacy unique-parent walk).

Per-rule telemetry on the post-Phase-1 canary
(`DUMP_REWRITE=1`):

- `inline-softmax-broadcast-reduce`: 27 hits per training-graph
  classify (vs 0 pre-Phase-1) - this is the single rule that
  drove the kernel-count drop;
- `inline-reduce-scalar-tail`: now occasionally fires (loss reduce);
- `inline-adjacent-reduce-chains`: occasionally fires (4 hits
  observed in one classify - the 3-axis BN reduce);
- `remove-by-cost-score`: still infrequent without
  tile-feasibility feedback, but fires 1-16x in some classifies
  now that the realize-rule order leaves more candidates eligible.

The 27 metal-op fallbacks split into two failure modes:

- 17 of them were merged kernels whose root was already a REDUCE
  (BN-mean / BN-var chains folding into a downstream maxpool/grad
  reduce) - inlining produced a 2-REDUCE program that
  `rangeify_try_lower_elementwise` rejected ("> 1 reduce").
  `realize_softmax_reduce_tile_cap` (default-on) now keeps these
  REDUCEs realized so the consumer kernel stays inside rangeify's
  single-reduce envelope.
- 10 still fall back, all bailing rangeify with
  `pre-reduce dim mismatch (non-broadcast)` -- a separate stride-
  view shape compatibility issue that needs a real rangeify fix
  rather than a planner gate.

**Phase 7 producer-consumer fusion -- low leverage on this canary.**
A diagnostic pass (now reverted) counted producer->consumer pairs
where boundary B's UOp subgraph reaches boundary A's loc, both
elementwise, op-budget OK.  Result on the post-Phase-1 graph:
**3 candidate pairs total** across all `thvm_materialize` calls
in a training step.  Implementing the splice would cut at most ~3
kernels for a substantial multi-day refactor.  The candidates are
absent because:

- Pure-elementwise pairs are already inlined by
  `remove-removable-bufferize` (single-use producers fuse into the
  consumer).
- Multi-use pairs typically have a REDUCE or movement op between
  them, which breaks both the elementwise predicate and the
  same-iter-shape predicate.
- The 27-fold sibling fusion path (`THVM_KERNEL_MERGE=1`) already
  catches the same-input fan-out case from a different angle.

A more leveraged variant would be **cross-reduce fusion** -- a
multi-output kernel that emits both A's reduced output AND B's
post-broadcast output in one dispatch.  That requires extending
the splice to handle differing iter shapes (a kernel with two
loop nests), which is structural work substantially beyond the
scope of "more rules".

Where the next measurable wins live:

- **Tile-feasibility cost model** (Phase 4 follow-up): predict
  whether a removal would force metal-jit fallback before
  firing.  A first-cut consumer-budget guard landed in
  `realize_remove_by_cost_score_consumers_fit_tile` (it walks
  parent chains up to the realized ancestor whose kernel would
  absorb the recompute and rejects when projected ops exceed
  budget).  The heuristic doesn't bite on beautiful-mnist
  because tile-feasibility actually depends on op shape /
  types / broadcast patterns, not just op count.  A precise
  predictor needs codegen→bufferize feedback: when materialize
  emits a kernel and chooses tile vs jit, propagate that
  decision back into the bufferize cost model so future passes
  can see "this kernel program shape was tile-supported" and
  generalize.  Without that, the rule's threshold has to stay
  conservative (T=50000 default) and the guard is mostly
  defensive scaffolding.
- **Generalised broadcast-reduce chain** (Phase 5): the chain
  predicate accepts only NEG/RECIP/SQRT/EXP2/LOG2; widening to
  cover the BatchNorm/Adam patterns would catch the REDUCEs
  that currently materialise on every step.
- **Group-reduce + accumulator schedules** (Phase 5): the
  TILE_REDUCE path already supports group axes; the bufferize
  plan would feed it richer reduce metadata so autotune can
  pick group sizes per-shape.
- **Memory-planning rerouting** (Phase 6): bufferize lifetimes
  match `BOUNDARY_LAST_USE` exactly (proven by the parity
  test); future rules that consult the canonical bufferize
  graph for alias / reuse / recompute decisions can run as
  bufferize-graph rewrites instead of materialize-time fixups.
- **Schedule cache** (Phase 7): `bufferize_schedule_key()`
  returns a stable hash; an in-memory cache keyed by it would
  let autotune reuse winning decisions across repeated train
  steps without re-searching.

These are all real work, not paraphrases of the plan; each
requires designing new infrastructure on top of the canonical
bufferize graph rather than another data-layer addition.

## Non-Goals

- Do not make custom GEMM, Conv, or FlashAttention kernels the main
  fusion strategy.
- Do not broaden graph simplification as a default materializer pre-pass
  until substitution-context rebuilds are proven transparent.
- Do not remove rangeify; bufferize should feed rangeify better
  edge-local context.
- Do not use broad sweeps to hide memory-planning bugs.
- Do not relax shape/range guards without a focused no-bail and
  numerical correctness test.

## Immediate Next Step

Phases 0-3 have landed (with the Phase 2 rangeify rerouting
deferred and the index-* rules tracked as accounting only).  The
next concrete work is the Phase 2 rangeify rerouting:

1. Add per-op data to `BIndex` (or a sibling `BIndexChain` table)
   so a record carries enough information to drive `RngsCtx`
   construction: per-axis dims for reshape, pad widths, axis
   permutations, flip masks.  Today only the boolean flags are
   stored.
2. Reroute `schedule/rangeify.c`'s `RngsCtx`/`KProgOp` chain walk
   so it reads from the bufferize edge table when one is
   available, and falls back to the heap walk otherwise.
3. Once rerouting is stable, turn each `index-*` rule into an
   actual edge transform (e.g. fold identity reshapes, collapse
   adjacent permutes, lift PAD masks to valid_masks) instead of a
   pure accounting rule.
4. Keep `THVM_RANGEIFY_BAIL=1` regression coverage in
   `rangeify_gaps.wlt`, `grad.wlt`, and `nn.wlt`; add focused
   no-bail tests for the shared-producer-multiple-views case
   end-to-end.

After this, Phase 4 (first-class bufferize removal with cost model)
becomes a clean per-edge analysis on the bufferize graph rather
than a side channel into `REALIZE_INFO`.
