# Bufferize Schedule IR Plan

Status: Phases 0-1 landed; Phases 2-7 planned.

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

### Movement-To-Index Rules

These rewrite consumer edges, not producers globally.

- `index-reshape`
- `index-permute`
- `index-expand`
- `index-pad-mask`
- `index-shrink`
- `index-flip`
- `index-contiguous-collapse`
- `index-load-mask-to-valid`

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

Tasks:

- generate one `B_INDEX` for every producer-to-consumer edge;
- encode reshape/pad/expand/shrink/flip/permutation context on the edge;
- make rangeify consume `B_INDEX` for direct loads and expression loads;
- preserve valid masks through `S_IWHERE`;
- add focused tests for shared producer with multiple movement views.

Acceptance:

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

### Phase 4: First-Class Bufferize Removal

Implement tinygrad-style `INDEX(BUFFERIZE(x)) -> INDEX(x)` rewrites
under legality gates.

Tasks:

- remove const/noop view buffers;
- remove pure single-use buffers;
- remove small pure multi-consumer buffers by recompute;
- add cost model inputs: op count, bytes, consumers, reduce depth,
  backend input count, local memory;
- add candidate dump sorted by potential kernel-count reduction and
  memory impact.

Acceptance:

- one-step beautiful-mnist kernel count drops materially from the
  current 1330-kernel canary;
- retained/live Metal memory does not grow;
- performance is not slower by more than noise on the bounded canary.

### Phase 5: Reduce-Aware Bufferize

Handle reductions as accumulator schedules, not unconditional
materialization points.

Tasks:

- represent accumulator buffers explicitly;
- fuse reduce chains through `B_STORE` reduce ranges;
- add `TILE_REDUCE` legalization over explicit reduce metadata;
- introduce local/group-reduce axes before MMA work;
- implement reduce-broadcast fusion for batchnorm and softmax-like
  patterns.

Acceptance:

- batchnorm channel reductions and beautiful-mnist backward reduce
  patterns stop dominating kernel count;
- Metal tile remains the dispatch path, not `metal-jit`;
- local/group reduce options are visible to autotune.

### Phase 6: Memory Planning Over Bufferize IR

Make memory planning consume schedule graph lifetimes instead of only
captured replay side effects.

Tasks:

- compute first/last use from `B_STORE` and replay graph;
- choose alias/reuse/recompute candidates before Metal allocation;
- preserve all outputs in a replay batch boundary;
- add memory pressure skip reasons to bufferize rewrites;
- keep broad sweeps disabled until bounded memory canaries are stable.

Acceptance:

- beautiful-mnist one-step retained memory moves toward tinygrad scale;
- no 100 GB pressure failure under bounded canaries;
- replay correctness survives multi-output and multi-grad graphs.

### Phase 7: Autotune Integration

Let autotune choose among legal bufferize/tile alternatives instead of
only choosing per-kernel axis opts.

Tasks:

- encode candidate schedules with stable keys;
- compare remove-vs-preserve-vs-split alternatives;
- feed tile options and memory estimates into one score;
- cache winning schedule choices by graph/kid/key;
- replay the selected schedule without re-searching.

Acceptance:

- bounded beautiful-mnist training loop can run autotune without memory
  blow-up;
- selected schedules reduce full-loop wall time, not just first-sample
  or compile-time metrics;
- canary output reports kernel count, graph dispatch count, memory, and
  training-step wall time after each run.

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

## Current Baseline

Latest bounded Metal/tile beautiful-mnist canary after the first
default `remove-removable-bufferize` rule:

- timed step: about `694 ms`;
- kernels: `1330`;
- dispatch: `metal-tile=1186`, `metal-alias=144`;
- retained Metal memory: about `2.65 GB`;
- live Metal memory: about `1.63 GB`.

This is better structurally than the previous 1386-kernel canary, but
not close enough.  The next reductions must come from explicit
`BUFFERIZE`/`INDEX` rewrites, reduce-aware scheduling, and memory-aware
autotune.

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

Phases 0-1 have landed; the next implementation step is Phase 2:

1. Generate one `B_INDEX` record per producer-to-consumer edge so
   reshape/pad/expand/shrink/flip/permute context is edge-local
   instead of producer-global.
2. Teach rangeify to read `B_INDEX` for direct loads, expression
   loads, and reduction inputs, preserving valid masks through
   `S_IWHERE` rather than recovering them from the original UOp
   graph.
3. Keep `THVM_RANGEIFY_BAIL=1` regression coverage and add focused
   tests where a shared producer is consumed under multiple
   movement views.

`B_INDEX` is what turns Phases 4-6 into editable rewrites: removal,
insertion, and memory rules all reason about consumer edges, not
producer-global state.  Until Phase 2 lands, those phases would have
to keep recovering edge contexts from the heap walk.
