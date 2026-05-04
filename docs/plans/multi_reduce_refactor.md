# Multi-Reduce-Per-Kernel Refactor

## Why

At BS=128 on the bounded beautiful-mnist canary, **84% of wall time
(575 / 687 ms)** comes from two metal-op fallback kernels that have
two `UOP_REDUCE` ops in their KProgOp programs.  The gate at
[src/schedule/rangeify.c:1410](src/schedule/rangeify.c) bails any
program with `> 1 reduce`, forcing them onto the per-op encoder:

```
case UOP_REDUCE:
  if (reduce_pos != -1) RBAIL_PRE("> 1 reduce");
  reduce_pos = (int)i;
  break;
```

These are BN-grad fused kernels (dgamma + dbeta from the same dy
over the same batch+spatial axes).  Tinygrad handles them in one
kernel via two threadgroup-shared accumulators in a shared inner
loop; that's the missing structural piece.

This refactor is also Phase 5 of [docs/plans/bufferize.md] ("Reduce-
Aware Bufferize").  Lifting the gate is the most concrete bit of
single-reduce assumption left in the post-Phase-7 pipeline.

## Pipeline impact

The refactor spans three layers:

```
KProgOp[] (legacy, on KernelEntry.program)
  ↓ rangeify  (semantic lowering)
ScalarUop[] (per-kernel scalar dataflow)
  ↓ tile_build_from_scalar  (schedule/memory plan)
TileUop[] (TILE_LOOP_NEST / TILE_REDUCE / TILE_STORE / TILE_SCALAR_BODY)
  ↓ render
Metal MSL / C source
```

| Layer | Current state | Needs |
|---|---|---|
| Rangeify | Bails at `> 1 reduce` (rangeify.c:1410); single-reduce metadata in scalars `red`, `reduce_kind`, `reduce_size`, `reduce_inner`, `reduce_n_ranges`, `reduce_extents`. ~119 refs to these. | Per-reduce arrays, emit N `S_REDUCE_SUM` ops, lift gate. |
| Tile | `tile_build_from_scalar` wraps ONE TILE_REDUCE; `tile_find_nested_scalar_reduce` discovers the other case (tile.c:942). | Wrap N TILE_REDUCEs, share inner loop when ranges match. |
| Render | `rmt_emit_value_with_reduce` emits ONE `_acc%u` register + reduce loop; substitutes acc into surrounding expression. | Emit N accumulators, merge inner loops when shared ranges. |

## Strategy

The refactor breaks into 5 atomic commits, each ending with `make
test` green:

### Commit 1 — Introduce per-reduce metadata in rangeify (no behavior change)

- Define a struct `ReduceRunMeta { i32 pos; KProgOp const *op; u32 kind; u32 size; u32 inner; u32 n_ranges; u32 extents[MAX_DIM]; }`.
- Define `MAX_REDUCES_PER_KERNEL = 4` (enough for BN-grad + a couple of related cases; can grow).
- Add `ReduceRunMeta reduces[MAX_REDUCES_PER_KERNEL]; u32 n_reduces = 0;` in `rangeify_try_lower_elementwise`.
- In the existing detect block, populate `reduces[0]` with the same data that goes into the existing scalars.  Keep the existing scalars as duplicate state for now.
- Keep the gate at `n_reduces > 1` (still bails).
- Stops behavior identical; just adds parallel state.

### Commit 2 — Migrate downstream reads to `reduces[0]`

- Find all `red->`, `reduce_kind`, `reduce_size`, etc. uses below the
  detect block.
- Replace with `reduces[0].op->`, `reduces[0].kind`, etc.
- Remove the now-redundant scalars.
- Behavior unchanged.  Pure mechanical refactor.

### Commit 3 — Allow multi-reduce shape detection

- In the gate, when `n_reduces > 1`, populate `reduces[1..N-1]` from
  the additional KProgOp `UOP_REDUCE` entries.
- For each, validate that `n_ranges` and `extents[]` match
  `reduces[0]` exactly (shared reduce-range axes).  If not,
  bail with `"multi-reduce ranges differ"` (a lighter restriction
  than the current blanket `> 1 reduce`).
- Still emit only one reduce in the body (use `reduces[0]`); just
  don't bail.  This lets the gate accept BN-grad's dgamma+dbeta but
  CONTINUES to emit only one of the reduces' value -- semantically
  WRONG.  Gated by `THVM_RANGEIFY_MULTI_REDUCE=1` so the canary
  default stays correct until Commit 4 lands.

### Commit 4 — Emit N S_REDUCE_SUM ops sharing the inner loop

- In the body emit pass, after computing the inner-reduce body
  expression, iterate `n_reduces`.  For each, compute its specific
  body subexpression (different inputs / pre-reduce ops) and emit
  one `S_REDUCE_SUM` consuming the shared reduce-range nest.
- Multi-output structure: each reduce becomes one of the kernel's
  output buffers (use the existing
  `KernelEntry.extra_output_*[]` schema introduced in step-1 of the
  multi-output kernel work).
- Tile build: extend `tile_build_from_scalar` to wrap each reduce
  in its own TILE_REDUCE that shares the same TILE_LOOP_NEST axes.
- Render: extend `rmt_emit_value_with_reduce` to walk N
  accumulators in the shared loop.

### Commit 5 — Make `THVM_RANGEIFY_MULTI_REDUCE=1` default-on

- Run BS=128 canary, confirm metal-op fallbacks lift to metal-tile.
- Run BS=32 canary, confirm no regression.
- Update `docs/plans/profiling_methodology.md` Tables A/B with new
  numbers.
- Flip default to ON.

## Side cleanups (orthogonal, can land any time)

- `KDISPATCH_METAL_GEMV = 12` — retired enum value, only the
  comment uses it.  Removing requires updating the WL-side
  `$dispatchKindNames` table at [wl/THVMLink/Kernel/Kernel.wl:281]
  too; otherwise the integer 12 maps to `"unknown"` after retirement.
- The "legacy" path comments in `materialize.c` for the
  single-output multi-output schema (around line 700) — the schema
  has shipped; some comments still describe it as "legacy single-
  output path", which is now the n=0-extras case rather than a
  legacy fallback.  Tighten language.
- `realize_classify` -> bufferize migration is the larger Phase
  5+ work tracked in [docs/plans/bufferize.md]; mention it in the
  "Future Work" section.

## Testing

- Each commit lands with `make test` green (274/274).
- Add focused unit tests in `tests/test_rangeify.c` (or extend an
  existing one) for the multi-reduce shapes:
  - Two REDUCE_SUM over the same axes from the same source (the
    cleanest BN-grad shape).
  - Two REDUCE_SUM with different bodies but shared ranges.
- Canary check at BS=32 (must stay competitive) and BS=128 (target
  metric: kid=6 / kid=9 lift to metal-tile, total wall drops to
  ~150ms).

## Risk / Bail Conditions

- If Commit 3's gate relaxation surfaces multi-reduce kernels with
  DIFFERENT reduce-range shapes (less-uniform than BN-grad), bail
  early; the simple shared-loop approach won't cover them.  Add
  a `n_reduces == 1` exit from rangeify and let those kernels fall
  through to per-op metal-op (existing behavior).  Don't try to
  cover the general case in this refactor.

## Diagnostic finding (after step 2-lite, commit 7c52f89)

Empirical pattern at BS=128 on the canary's two metal-op fallbacks
(kid=6, kid=9): the reduces are NOT parallel-with-shared-axes.
They fail the `multi-reduce source/axes differ` compat check --
each reduce has its own source operand and reduce-axis layout.
This is the **chain-reduce pattern**: reduce 1 produces an
intermediate that feeds reduce 2's body via post-process ops.

Concretely the per-thread emission for chain reduces needs:

```
for each output position:
  acc1 = 0
  for each reduce_1 axis:                   # inner loop 1
    acc1 += compute_body_1(input)
  intermediate = post_process_1(acc1)
  acc2 = 0
  for each reduce_2 axis:                   # inner loop 2
    acc2 += compute_body_2(intermediate, input)
  result = post_process_2(acc2)
  store(result)
```

Two SEQUENTIAL inner loops per thread, not one shared one.

The rangeify per-thread emission (around `rangeify.c:3185`) walks
the program once with categorical pre / reduce / post scopes
demarcated by the SINGLE `reduce_pos`.  To support chain reduces,
each `UOP_REDUCE` becomes its own scope boundary; the program
walks N+1 phases (pre-1, reduce-1, mid-1-2, reduce-2, post-2).
Variables like `prog_value[]` get repopulated per reduce-scope.

This is the actual architectural lift for this canary's BS=128
slowdown.  Step 4 of the plan above needs to be expanded to:

  4a. Generalise the categorical scope variables (`pre`, `post`,
      `prog_value[i]`'s scope-aware indexing) to N reduces.
  4b. Emit N sequential inner reduce loops in the per-thread
      kernel body.  Each reduce loop has its own accumulator
      register; the value flows through subsequent ops to feed
      the next reduce or the final store.
  4c. Tile build_from_scalar wraps each reduce in its own
      TILE_REDUCE; the TILE_LOOP_NEST has all of them as direct
      children of the TILE_STORE chain (sequential, not nested).
  4d. Renderer's rmt_emit_value_with_reduce already handles a
      single nested reduce; extend to walk N reduces in sequence.

This is multi-day and the most invasive piece of the multi-reduce
refactor.  The shared-source compat check from step 2-lite
remains useful for the much-simpler parallel case if it surfaces
in other workloads (BN forward training-time mean+var when those
both reduce x over the same axes -- doesn't seem to occur in the
beautiful-mnist canary because of how realize-classify schedules
those reduces into separate kernels).

## Verified shapes at BS=128 (2026-05-03 dump)

The two metal-op fallbacks are confirmed BN-backward residual chains.
Dumped via [/tmp/dump_multi_reduce.wls] against the canary at BS=128
WARMUP=1 N_STEPS=1.

**kid=6** (13 ops, 2 REDUCEs):

```
[1-5]  pre = RESHAPE/PERMUTE/ADD/CMPLT/MUL → KOp[4] (numel=1638400, relu mask × dy)
[6]    REDUCE KOp[4]            arg=1            → numel=32   (reduce 1: dbeta)
[7-9]  post1 = MUL × KIn[3], RESHAPE, EXPAND     → numel=1638400
[10]   MUL KOp[8] × KIn[4]                       → numel=1638400
[11]   ADD KOp[9] + KOp[4]      ← residual: combines post-1 broadcast with PRE-reduce-1 KOp[4]
[12]   MUL KOp[10] × KOp[10]                     → numel=1638400
[13]   REDUCE KOp[11]           arg=1            → numel=81920 (reduce 2: 1-axis factor 20)
```

**kid=9** (22 ops, 2 REDUCEs):

```
[1-11] pre = compute relu mask × scaled-dy → KOp[10] (numel=1638400)
[12]   REDUCE KOp[10]           arg=1            → numel=32   (reduce 1)
[13-15] post1 = MUL × KIn[1], RESHAPE, EXPAND    → numel=1638400
[16]   MUL KOp[14] × KIn[6]                      → numel=1638400
[17]   ADD KOp[15] + KOp[10]    ← residual: post-1 + PRE-reduce-1 KOp[10]
[18-21] further muls with running 1/sqrt etc.
[22]   REDUCE KOp[20]           arg=16777236     → numel=819200 (reduce 2: 1-axis factor 2)
```

Two structural features both kernels share:

1. **Reduce 2's body subgraph reads reduce 1's output** (via
   [9]→broadcast→[10]→[11] in kid=6, [15]→[17]→[18]→[19]→[20] in kid=9).
   Pure chain dependency — reduce 1 must complete before reduce 2's
   inner loop body can be evaluated.
2. **Reduce 2's body also reuses the pre-reduce-1 source** (KOp[4] in
   kid=6, KOp[10] in kid=9) via a residual ADD with the post-reduce-1
   broadcast.  The per-thread emission must therefore re-load (or
   recompute) the pre-reduce-1 expression in reduce 2's inner loop.

That second property is what makes the simple "shared inner loop"
approach impossible for these kernels.  Reduce 1 collapses many axes
(NCHW→C, factor 51200) while reduce 2 collapses only one (factor 20
or factor 2).  The reduce loops aren't even over the same axis-shape,
so they can't be merged.  Sequential inner loops with the pre-reduce
expressions re-loaded in reduce 2's body is the only correct shape.

Implication for step 4: the ScalarUop-level body emit needs a
"region map" -- for each KProgOp index, which reduce-region does it
belong to (pre-1, in-reduce-1, mid-1-2, in-reduce-2, post-2).  The
existing `int pre = (has_reduce && (int)i <= reduce_pos)` boolean
becomes a `region[i] ∈ {0..2N}` array.
